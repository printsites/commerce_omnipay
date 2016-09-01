<?php

use Omnipay\Omnipay;

function commerce_omnipay_omnipay_settings_form($settings) {
  $form = array();
  return $form;
}

function commerce_omnipay_omnipay_submit_form() {
  module_load_include('inc', 'commerce_payment', 'includes/commerce_payment.credit_card');
  $active_gateway = variable_get("commerce_omnipay_active_gateway", "");
  $all_gateways = commerce_omnipay_supported_gateways();
  // Ensure active gateway is actually valid.
  if (isset($all_gateways[$active_gateway])) {
    $credit_card_fields = array(
      'owner' => '',
      'number' => '',
      'exp_month' => '',
      'exp_year' => '',
      'code' => '',
    );

    $form = commerce_payment_credit_card_form($credit_card_fields);

  }
  else {
    $form['payment_error'] = array(
      '#type' => "markup",
      '#markup' => "This website does not require payment",
    );
  }

  return $form;
}

function commerce_omnipay_omnipay_submit_form_validate() {
  // todo Add validation.
}

function commerce_omnipay_omnipay_submit_form_submit($payment_method, $pane_form, $pane_values, $order, $charge) {
  global $user;

  $active_gateway = variable_get("commerce_omnipay_active_gateway", "");
  $all_gateways = commerce_omnipay_supported_gateways();

  // Ensure active gateway is actually valid.
  if (isset($pane_values['credit_card'])) {
    $gateway = Omnipay::create($all_gateways[$active_gateway]['gateway']);
    $current_params = commerce_omnipay_get_gateway_settings($active_gateway);
    $gateway->initialize($current_params);

    $transaction = commerce_payment_transaction_new($payment_method['method_id'], $order->order_id);
    $transaction->instance_id = $payment_method['instance_id'] . '|' . $all_gateways[$active_gateway]['gateway'];
    $transaction->amount = $charge['amount'];
    $transaction->currency_code = $charge['currency_code'];
    $transaction->data['gateway'] = $active_gateway;

    $data = commerce_omnipay_set_billing_data($order, $data = []);
    $data = commerce_omnipay_set_shipping_data($order, $data);
    $data = commerce_omnipay_set_card_data($pane_values, $data);

    $data['clientIp'] = ip_address();
    $data['email'] = $user->mail;
    $data['transactionId'] = $order->order_id;

    $transactionAmount = number_format(commerce_currency_amount_to_decimal($charge['amount'], $charge['currency_code']), 2);

    if (!isset($current_params['payment'])) $current_params['payment'] = 'auth_cap';

    switch ($current_params['payment']) {
      case 'auth_only':
        $method = 'authorize';
        $transType = COMMERCE_CREDIT_AUTH_ONLY;
        break;

      case 'auth_cap':
      default:
        $method = 'purchase';
        $transType = COMMERCE_CREDIT_AUTH_CAPTURE;
        break;
    }

    try {
      $response = $gateway->{$method}(
        [
          'amount' => $transactionAmount,
          'currency' => $charge['currency_code'],
          'card' => $data,
        ]
      )->send();
    } catch (Exception $e) {
      drupal_set_message($e->getMessage());
    }

    $log = commerce_omnipay_log_transaction([
      'pane_values' => $pane_values,
      'charge' => $charge,
      'response' => $response,
      'transactionAmount' => $transactionAmount,
      'transType' => $transType,
    ]);

    // Process response.
    if ($response->isSuccessful()) {
      $transaction->remote_id = $response->getTransactionReference();
      $transaction->message = t($method . ' completed successfully.');
      $transaction->status = ('authorize' == $method ? COMMERCE_PAYMENT_STATUS_PENDING : COMMERCE_PAYMENT_STATUS_SUCCESS);
      $transaction->remote_status = $method;
      $log['GatewayParameter'] = $current_params;
      $transaction->payload[REQUEST_TIME] = $log;
      $transaction->message_variables = $log;
      commerce_payment_transaction_save($transaction);
    }
    else {
      drupal_set_message($response->getMessage());
      return FALSE;
    }
  }
  return TRUE;
}

/**
 * @param $action - refund or void
 * @param $transaction
 * @param $refund_amount
 *
 * Refunds or voids the transaction as specified by $action
 */
function commerce_omnipay_refund_void($action, $transaction, $refund_amount) {
  $active_gateway = variable_get("commerce_omnipay_active_gateway", "");
  $all_gateways = commerce_omnipay_supported_gateways();

  $gateway = Omnipay::create($all_gateways[$active_gateway]['gateway']);
  $current_params = commerce_omnipay_get_gateway_settings($active_gateway);
  $gateway->initialize($current_params);

  if ('refund' == $action) {
    $data = [
      'amount' => $refund_amount,
      'currency' => $transaction->currency_code,
      'transactionReference' => $transaction->message_variables['TransReference'],
    ];
  }
  else {
    // assume action == void
    $data = [
      'currency' => $transaction->currency_code,
      'transactionReference' => $transaction->message_variables['TransReference'],
    ];
  }

  try {
    $response = $gateway->{$action}($data)->send();
  } catch (Exception $e) {
    drupal_set_message($e->getMessage());
  }

  if ('refund' == $action) {
    $transType = COMMERCE_CREDIT_CREDIT;
    $log_data = [
      'transaction' => $transaction,
      'response' => $response,
      'refund_amount' => $refund_amount,
      'transType' => $transType,
    ];
  }
  else {
    // assume action == void
    $transType = COMMERCE_CREDIT_VOID;
    $log_data = [
      'transaction' => $transaction,
      'response' => $response,
      'transType' => $transType,
    ];
  }
  $log = commerce_omnipay_log_transaction($log_data);

  if ($response->isSuccessful()) {
    drupal_set_message(t('Credit for @amount issued successfully', array('@amount' => commerce_currency_format($refund_amount * 100, $transaction->currency_code))));

    // Create a new transaction to record the credit.
    $credit_transaction = commerce_payment_transaction_new($transaction->instance_id, $transaction->order_id);
    $credit_transaction->instance_id = $transaction->instance_id;
    $credit_transaction->remote_id = $response->getTransactionReference();
    if ('refund' == $action) {
      $credit_transaction->amount = $refund_amount * -1 * 100;  // * 100 to account for the fact that DC stores amounts in cents.
    }
    $credit_transaction->currency_code = $transaction->currency_code;
    $log['GatewayParameter'] = $current_params;
    $credit_transaction->payload[REQUEST_TIME] = $log;
    $credit_transaction->message_variables = $log;
    $credit_transaction->status = COMMERCE_PAYMENT_STATUS_SUCCESS;
    $credit_transaction->remote_status = ('refund' == $action ? 'refund' : 'void');
    if ('refund' == $action) {
      $credit_transaction->message = t('Credited to @remote_id.', array('@remote_id' => $transaction->remote_id));
    }
    else {
      // assume action == void
      $credit_transaction->message = t('Voided @remote_id.', array('@remote_id' => $transaction->remote_id));
    }

    // Save the credit transaction.
    commerce_payment_transaction_save($credit_transaction);
  }
  else {
    // Save the failure response message to the original transaction.
    $transaction->payload[REQUEST_TIME] = $response;
    drupal_set_message(t('Refund/Void failed.'));
    commerce_payment_transaction_save($transaction);
  }
}

function commerce_omnipay_capture($transaction, $order_total) {
  $active_gateway = variable_get("commerce_omnipay_active_gateway", "");
  $all_gateways = commerce_omnipay_supported_gateways();

  $gateway = Omnipay::create($all_gateways[$active_gateway]['gateway']);
  $current_params = commerce_omnipay_get_gateway_settings($active_gateway);
  $gateway->initialize($current_params);

  // get transaction data
//  $transaction = commerce_payment_transaction_load($transactionId); //$form_state['transaction'];
  $transaction_ref = $transaction->message_variables['TransReference'];
  $transactionAmount = $order_total;

  try {
    $response = $gateway->capture(
      [
        'transactionReference' => $transaction_ref,
        'amount' => $transactionAmount,
        'currency' => $transaction->currency_code,
      ]
    )->send();
  } catch (Exception $e) {
    drupal_set_message($e->getMessage());
  }

  $transType = COMMERCE_CREDIT_PRIOR_AUTH_CAPTURE;
  $log = commerce_omnipay_log_transaction([
    'response' => $response,
    'transactionAmount' => $transactionAmount,
    'transType' => $transType,
  ]);

  // Process response.
  if ($response->isSuccessful()) {
    $trans_data = $response->getData();
    $transaction->amount = $trans_data['amount'] - $trans_data['amount_refunded'];
    $transaction->remote_id = $response->getTransactionReference();
    $transaction->message = t('Capture completed successfully.');
    $transaction->status = COMMERCE_CREDIT_PRIOR_AUTH_CAPTURE;
    $transaction->remote_status = $trans_data['status'];
    $log['GatewayParameter'] = $current_params;
    $transaction->payload[REQUEST_TIME] = $log;
    $transaction->message_variables = $log;
    commerce_payment_transaction_save($transaction);
  }
  else {
    // Save the failure response message to the original transaction.
    $transaction->payload[REQUEST_TIME] = $response;
    drupal_set_message(t('Capture failed.'));
    commerce_payment_transaction_save($transaction);
  }
}

function commerce_omnipay_log_transaction($parameters) {
  $log = [
    'Successful' => $parameters['response']->isSuccessful(),
    'ReasonCode' => $parameters['response']->getCode(),
    'Message' => $parameters['response']->getMessage(),
    'AVSCode' => (method_exists($parameters['response'], 'getAVSCode') ? $parameters['response']->getAVSCode() : ""),
    'TransReference' => $parameters['response']->getTransactionReference(),
    'TransactionType' => $parameters['transType'],
  ];

  switch ($parameters['transType']) {
    case COMMERCE_CREDIT_AUTH_CAPTURE:
      $log['RequestData'] = [
        'currency' => $parameters['charge']['currency_code'],
        'amount' => $parameters['transactionAmount'],
        'last_four' => substr((string) $parameters['pane_values']['credit_card']['number'], -4),
      ];
      break;

    case COMMERCE_CREDIT_AUTH_ONLY:
      // todo add logic
      $log['RequestData'] = [
        'currency' => $parameters['charge']['currency_code'],
        'amount' => $parameters['transactionAmount'],
        'last_four' => substr((string) $parameters['pane_values']['credit_card']['number'], -4),
      ];
      break;

    case COMMERCE_CREDIT_PRIOR_AUTH_CAPTURE:
      $log['RequestData'] = [
        'currency' => $parameters['response']->getData()['currency'],
        'amount' => $parameters['transactionAmount'],
      ];
      break;

    case COMMERCE_CREDIT_CREDIT:
      $log['RequestData'] = [
        'refund_amount' => $parameters['refund_amount'],
      ];
      break;
  }

  // todo What other info about the request should be logged?
  watchdog("Commerce Omnipay", "<pre>" . json_encode($log) . "</pre>");

  return $log;
}

function commerce_omnipay_set_card_data($pane_values, $data) {
  $data['number'] = $pane_values['credit_card']['number'];
  $data['expiryMonth'] = $pane_values['credit_card']['exp_month'];
  $data['expiryYear'] = $pane_values['credit_card']['exp_year'];
  $data['cvv'] = $pane_values['credit_card']['code'];
  return $data;
}

function commerce_omnipay_set_shipping_data($order, $data) {
  $shipping = FALSE;
  if (isset($order->commerce_customer_shipping['und'][0]['profile_id'])) {
    $shipping = commerce_customer_profile_load($order->commerce_customer_shipping['und'][0]['profile_id']);
  } //TODO: Remove this.
  elseif (isset($order->commerce_customer_sf_shipping['und'][0]['profile_id'])) {
    $shipping = commerce_customer_profile_load($order->commerce_customer_sf_shipping['und'][0]['profile_id']);
  }

  if ($shipping) {
    $data['shippingAddress1'] = $shipping->commerce_customer_address['und'][0]['thoroughfare'];
    $data['shippingAddress2'] = $shipping->commerce_customer_address['und'][0]['premise'];
    $data['shippingCity'] = $shipping->commerce_customer_address['und'][0]['locality'];
    $data['shippingPostcode'] = $shipping->commerce_customer_address['und'][0]['postal_code'];
    $data['shippingState'] = $shipping->commerce_customer_address['und'][0]['administrative_area'];
    $data['shippingCountry'] = $shipping->commerce_customer_address['und'][0]['country'];
  }
  return $data;
}

function commerce_omnipay_set_billing_data($order, $data) {
  if (isset($order->commerce_customer_billing['und'][0]['profile_id'])) {
    $billing = commerce_customer_profile_load($order->commerce_customer_billing['und'][0]['profile_id']);
    if ($billing) {
      $data['firstName'] = $billing->commerce_customer_address['und'][0]['first_name'];
      $data['lastName'] = $billing->commerce_customer_address['und'][0]['last_name'];
      $data['billingAddress1'] = $billing->commerce_customer_address['und'][0]['thoroughfare'];
      $data['billingAddress2'] = $billing->commerce_customer_address['und'][0]['premise'];
      $data['billingCity'] = $billing->commerce_customer_address['und'][0]['locality'];
      $data['billingPostcode'] = $billing->commerce_customer_address['und'][0]['postal_code'];
      $data['billingState'] = $billing->commerce_customer_address['und'][0]['administrative_area'];
      $data['billingCountry'] = $billing->commerce_customer_address['und'][0]['country'];

      //
      // TODO: this field is required by some merchants but is too specific for the printsites platform
      // and needs to be removed in some way.
      if (isset($billing->field_phone['und'][0]['value'])) {
        $data['billingPhone'] = $billing->field_phone['und'][0]['value'];
      }
    }
  }
  return $data;
}

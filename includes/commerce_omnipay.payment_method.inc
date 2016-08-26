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

    // todo remove this next line
    if (!isset($current_params['payment'])) $current_params['payment'] = 'purchase';

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

    $log = commerce_omnipay_log_transaction($pane_values, $charge, $response, $transactionAmount, $transType);

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

function commerce_omnipay_log_transaction($pane_values, $charge, $response, $transactionAmount, $transType) {
  $log = [
    'Successful' => $response->isSuccessful(),
    'ReasonCode' => $response->getCode(),
    'Message' => $response->getMessage(),
    'AVSCode' => (method_exists($response, 'getAVSCode') ? $response->getAVSCode() : ""),
    'TransReference' => $response->getTransactionReference(),
    'TransactionType' => $transType,
  ];

  switch ($transType) {
    case COMMERCE_CREDIT_AUTH_CAPTURE:
      $log['RequestData'] = [
        'currency' => $charge['currency_code'],
        'amount' => $transactionAmount,
        'last_four' => substr((string) $pane_values['credit_card']['number'], -4),
      ];
      break;

    case COMMERCE_CREDIT_CREDIT:
      $log['RequestData'] = [
        'refund_amount' => $transactionAmount,
      ];
      break;

    case COMMERCE_CREDIT_AUTH_ONLY:
      // todo add logic
      break;

    case COMMERCE_CREDIT_PRIOR_AUTH_CAPTURE:
      // todo add logic
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

      if (isset($billing->field_phone['und'][0]['value'])) {
        $data['billingPhone'] = $billing->field_phone['und'][0]['value'];
      }
    }
  }
  return $data;
}

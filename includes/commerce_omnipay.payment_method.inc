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

}

function commerce_omnipay_omnipay_submit_form_submit($payment_method, $pane_form, $pane_values, $order, $charge) {
  $active_gateway = variable_get("commerce_omnipay_active_gateway", "");
  $all_gateways = commerce_omnipay_supported_gateways();

  // Ensure active gateway is actually valid.
  if (isset($pane_values['credit_card'])) {
    $gateway = Omnipay::create($all_gateways[$active_gateway]['gateway']);
    $current_params = commerce_omnipay_get_gateway_settings($active_gateway);
    $gateway->initialize($current_params);

    $transaction = commerce_payment_transaction_new('commerce_stripe', $order->order_id);
    $transaction->instance_id = $payment_method['instance_id'];
    $transaction->amount = $charge['amount'];
    $transaction->currency_code = $charge['currency_code'];
    $transaction->data['gateway'] = $active_gateway;

    try {
      $data = [
        'number' => $pane_values['credit_card']['number'],
        'expiryMonth' => $pane_values['credit_card']['exp_month'],
        'expiryYear' => $pane_values['credit_card']['exp_year'],
        'cvv' => $pane_values['credit_card']['code']
      ];

      if (isset($order->commerce_customer_billing['und'][0]['profile_id'])) {
        $billing = commerce_customer_profile_load($order->commerce_customer_billing['und'][0]['profile_id']);
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

      $shipping = FALSE;
      if (isset($order->commerce_customer_shipping['und'][0]['profile_id'])) {
        $shipping = commerce_customer_profile_load($order->commerce_customer_sf_shipping['und'][0]['profile_id']);
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
      $response = $gateway->purchase(
        [
          'amount' => $charge['amount'] / 100,
          'currency' => $charge['currency_code'],
          'card' => $data,
        ]
      )->send();
      // Process response.
      if ($response->isSuccessful()) {
        watchdog("Commerce Omnipay", "<pre>" . print_r($response, TRUE) . "</pre>");
        $transaction->remote_id = $response->getTransactionReference();
        $transaction->message = t('Payment completed successfully.');
        $transaction->status = COMMERCE_PAYMENT_STATUS_SUCCESS;
        commerce_payment_transaction_save($transaction);
      }
      else {
        watchdog("Commerce Omnipay", "<pre>" . print_r($response, TRUE) . "</pre>");
        drupal_set_message($response->getMessage());
        return FALSE;
      }
    } catch (Exception $e) {
      watchdog("Commerce Omnipay", "<pre>" . print_r($e->getMessage(), TRUE) . "</pre>");
      drupal_set_message($e->getMessage());
      return FALSE;

    }
  }
}

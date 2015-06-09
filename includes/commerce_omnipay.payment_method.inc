<?php

use Omnipay\Omnipay;

function commerce_omnipay_omnipay_settings_form($settings) {
  $form = array();
  return $form;
}

function commerce_omnipay_omnipay_submit_form() {
  module_load_include('inc', 'commerce_payment', 'includes/commerce_payment.credit_card');

  $credit_card_fields = array(
    'owner' => '',
    'number' => '',
    'exp_month' => '',
    'exp_year' => '',
    'code' => '',
  );

  $form = commerce_payment_credit_card_form($credit_card_fields);

  return $form;
}

function commerce_omnipay_omnipay_submit_form_validate() {

}

function commerce_omnipay_omnipay_submit_form_submit($payment_method, $pane_form, $pane_values, $order, $charge) {
  dsm($charge);
  dsm($pane_values);
  dsm($payment_method);

  $active_gateway = variable_get("commerce_omnipay_active_gateway", "");
  $all_gateways = commerce_omnipay_supported_gateways();

  $gateway = Omnipay::create($all_gateways[$active_gateway]['gateway']);
  $current_params = commerce_omnipay_get_gateway_settings($active_gateway);
  $gateway->initialize($current_params);

  watchdog("SUP", "<pre>" . print_r($charge, true) . "</pre>");
  watchdog("SUP", "<pre>" . print_r($pane_values, true) . "</pre>");
  watchdog("SUP", "<pre>" . print_r($payment_method, true) . "</pre>");


  $data = [
    'number' => $pane_values['credit_card']['number'],
    'expiryMonth' => $pane_values['credit_card']['exp_month'],
    'expiryYear' => $pane_values['credit_card']['exp_year'],
    'cvv' => $pane_values['credit_card']['code']
  ];
  try {
    $response = $gateway->purchase(
      [
        'amount' => $charge['amount'] / 100,
        'currency' => $charge['currency_code'],
        'card' => $data,
      ]
    )->send();

    // Process response
    if ($response->isSuccessful()) {
      watchdog("Commerce Omnipay", "<pre>" . print_r($response, TRUE) . "</pre>");
    }
    elseif ($response->isRedirect()) {
      watchdog("Commerce Omnipay", "<pre>" . print_r($response, TRUE) . "</pre>");
    }
    else {
      watchdog("Commerce Omnipay", "<pre>" . print_r($response, TRUE) . "</pre>");
    }

  }
  catch (Exception $e) {
    watchdog("Commerce Omnipay", "<pre>" . print_r($e->getMessage(), TRUE) . "</pre>");
  }


}

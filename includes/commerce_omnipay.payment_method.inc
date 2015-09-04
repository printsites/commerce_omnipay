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

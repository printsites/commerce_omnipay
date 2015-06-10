<?php

use Omnipay\Omnipay;

function commerce_omnipay_settings_page() {
  return drupal_get_form("commerce_omnipay_settings_form");
}

/**
 * @param $form
 * @param $form_state
 * @return array
 */
function commerce_omnipay_settings_form($form, &$form_state) {
  $form = array();
  $gateways = commerce_omnipay_supported_gateways();
  $gateway_options = array();
  foreach ($gateways as $key => $value) {
    $gateway_options[$key] = $value['label'];
  }
  $active_gateway = variable_get("commerce_omnipay_active_gateway", "");
  $form['omnipay_active_gateway'] = array(
    '#title' => "Active Gateway",
    '#required' => TRUE,
    '#type' => "select",
    '#options' => $gateway_options,
    '#default_value' => $active_gateway,
  );

  foreach ($gateways as $key => $gateway_info) {
    $form[$key] = array(
      '#type' => "fieldset",
      '#title' => "Settings For Gateway: " . $gateway_info['label'],
      '#tree' => TRUE,
      '#collapsible' => FALSE,
      '#collapsed' => FALSE,
      '#states' => array(
        'visible' => array(
          ':input[name="omnipay_active_gateway"]' => array('value' => $key),
        ),
      ),
    );
    $gateway = Omnipay::create($gateway_info['gateway']);
    $default_params = $gateway->getDefaultParameters();
    $current_params = commerce_omnipay_get_gateway_settings($key);
    foreach ($default_params as $parm_key => $parm_value) {
      $default_value = (isset($current_params[$parm_key]) ? $current_params[$parm_key] : $default_params[$parm_key]);
      $var_type = gettype($parm_value);

      switch ($var_type) {
        case 'boolean':
          $form[$key][$parm_key] = array(
            '#type' => "select",
            '#title' => $parm_key,
            '#default_value' => $default_value,
            '#options' => array(
              '1' => "Yes",
              '0' => "No",
            ),
          );
          break;

        case 'string':
          $form[$key][$parm_key] = array(
            '#type' => "textfield",
            '#title' => $parm_key,
            '#default_value' => $default_value,
          );
          break;

      }
    }
  }

  $form['submit'] = array(
    '#type' => 'submit',
    '#value' => 'Submit',
  );

  return $form;
}

function commerce_omnipay_settings_form_validate($form, &$form_state) {

}

function commerce_omnipay_settings_form_submit($form, &$form_state) {
  variable_set("commerce_omnipay_active_gateway", $form_state['values']['omnipay_active_gateway']);
  $gateways = commerce_omnipay_supported_gateways();
  foreach ($gateways as $key => $gateway_info) {
    $gateway_settings = $form_state['values'][$key];
    $gateway = Omnipay::create($gateway_info['gateway']);
    $default_params = $gateway->getDefaultParameters();
    if ($gateway_settings != $default_params) {
      commerce_omnipay_set_gateway_settings($key, $gateway_settings);
    }
  }
  drupal_set_message("Payment gateway settings updated");
}
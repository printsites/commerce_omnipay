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
  $gateways = commerce_omnipay_supported_gateways();
  $active_gateway = variable_get("commerce_omnipay_active_gateway", "");

  $form = [
    'heading' => [
      '#markup' => '<h2>Configure Payment Gateway</h2>',
    ],
    'description' => [
      '#markup' => '<p>In order to accept credit card payments, enable and configure a payment gateway below.</p>',
      '#attributes' => [
        'class' => [
          'configure_payment',
        ],
      ],
    ],
    '#attached' => [
      'js' => [
        [
          'type' => 'file',
          'data' => drupal_get_path('module', 'commerce_omnipay') . '/js/commerce_omnipay.js',
        ]
      ],
      'css' => [
        [
          'type' => 'file',
          'data' => drupal_get_path('module', 'commerce_omnipay') . '/css/commerce_omnipay.css',
        ]
      ],
    ],
  ];

  foreach ($gateways as $key => $gateway_info) {
    $show_body = strcasecmp($key, $active_gateway) == 0;
    $enable = (isset($form_state['input'][$key]['enable']) ? TRUE : FALSE);
    $form[$key] = [
      '#type' => "fieldset",
      '#title' => $gateway_info['label'],
      '#tree' => TRUE,
      '#collapsible' => TRUE,
      '#collapsed' => FALSE,
      '#attributes' => [
        'class' => [
          'collapsible',
          (($show_body or $enable) ? "show_body" : NULL),
        ],
      ],
      'help' => [
      ],
      'enable' => [
        '#type' => 'checkbox',
        '#attributes' => [
          'class' => [
            'payment_gateway',
          ],
        ],
        '#title' => 'Enable ' . $gateway_info['label'],
        '#default_value' => (!strcasecmp($active_gateway,$key) ? TRUE : FALSE),
      ],
    ];

    $gateway = Omnipay::create($gateway_info['gateway']);
    $default_params = $gateway->getDefaultParameters();
    $current_params = commerce_omnipay_get_gateway_settings($key);
    foreach ($default_params as $parm_key => $parm_value) {
      $default_value = (isset($current_params[$parm_key]) ? $current_params[$parm_key] : $default_params[$parm_key]);
      $var_type = gettype($parm_value);
      $label = "";
      preg_match_all('/((?:^|[A-Z])[a-z]+)/',$parm_key,$label);
      $label = ucwords(implode(" ", array_pop($label)));

      switch ($var_type) {
        case 'boolean':
          $form[$key][$parm_key] = array(
            '#type' => "select",
            '#title' => $label,
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
            '#title' => $label,
            '#default_value' => $default_value,
          );
          break;

      }
    }

    $form[$key]['payment'] = [
      '#type' => 'radios',
      '#title' => t('When do you want to capture the payment from your customer?'),
      '#default_value' => (isset($current_params['payment']) ? $current_params['payment'] : 'auth_cap'),
      '#options' => [
        'auth_cap' => t('Capture payment at time of checkout'),
        'auth_only' => t('Capture payment when order completed in your Projects Queue.')
      ],
    ];
  }

  //
  // TODO: Remove.
  $form['#full_page_form'] = TRUE;

  $form['submit'] = [
    '#type' => 'submit',
    '#value' => 'Save',
    '#suffix' => l("Cancel", 'admin/commerce/payment'),
  ];

  return $form;
}

function commerce_omnipay_settings_form_validate($form, &$form_state) {
  $active = commerce_omnipay_get_active_gateway($form_state);

  if (isset($active) and isset($form_state['values'][$active])) {
    foreach ($form_state['values'][$active] as $key => &$value) {
      $value = trim($value);
      if ("" == $value) {
        //Could use a better error message.
        form_set_error($key, "Please provide a value for " . $key);
      }
    }
  }
}

function commerce_omnipay_settings_form_submit($form, &$form_state) {
  $active = commerce_omnipay_get_active_gateway($form_state);
  variable_set("commerce_omnipay_active_gateway", $active);
  $gateways = commerce_omnipay_supported_gateways();

  foreach ($gateways as $key => $gateway_info) {
    $gateway_settings = $form_state['values'][$key];
    $gateway = Omnipay::create($gateway_info['gateway']);
    $default_params = $gateway->getDefaultParameters();
    if ($gateway_settings != $default_params) {
      commerce_omnipay_set_gateway_settings($key, $gateway_settings);
    }
  }

  drupal_set_message("Payment settings updated");
  commerce_payment_methods_reset();
  //entity_defaults_rebuild();
  $form_state['redirect'] = 'admin/commerce/payment';
}

/**
 * @param $form_state
 * @return $key
 */
function commerce_omnipay_get_active_gateway($form_state) {
  $gateways = commerce_omnipay_supported_gateways();

  foreach ($gateways as $key => $value) {
    if (TRUE == $form_state['values'][$key]['enable']) {
      return $key;
    }
  }

  return NULL;
}

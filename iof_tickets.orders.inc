<?php

// $Id$

/**
 * Order handling for the IO-Festival tickets store.
 *
 */

/**
 * This form is used to lookup orders for a given identifier.
 *
 * The identifier can be one of the following:
 * - A student number.
 * - An i.d-card number.
 * - A campus card number.
 */
function iof_tickets_customer_lookup_form(&$form_state, $customer = NULL, $inline = FALSE) {
  $form = array(
    'lookup' => array(
      '#type' => 'fieldset',
      '#title' => t('Find customer'),
      '#collapsible' => TRUE,
      '#collapsed' => FALSE,
      'scan' => array(
        '#title' => t('Identifier'),
        '#type' => 'textfield',
        '#required' => TRUE,
        '#default_value' => ($customer->cardnumber !== NULL) ? $customer->cardnumber : $customer->studentnumber,
      ),
      'submit' => array(
        '#value' => t('Track orders'),
        '#type' => 'submit',
      ),
    )
  );

  return ($inline) ? $form : $form['lookup'];
}

/**
 * Validate the identifier that was given and determine the type.
 * Then proceed to lookup possible connected user information.
 *
 * If no system user is attached to the identifier, set uid to 0.
 * Store the known identifier.
 */
function iof_tickets_customer_lookup_form_validate($form, &$form_state) {
  $cardnumber = $studentnumber = NULL;
  // Parse scan input. Can be eithe campus card number or student number
  // i.d card scan: 36708XXXXXXXx
  if (preg_match('#^36708([0-9]{7})[0-9]$#', $form_state['values']['scan'], $matches)) {
    $studentnumber = $matches[1];
  }
  // campus card scan: xxxxxXXXXXXXXX
  elseif (preg_match('#[0-9]*([0-9]{9})$#', $form_state['values']['scan'], $matches)) {
    $cardnumber = $matches[1];
  }
  // student number, direct input
  elseif (preg_match('#([0-9]{7})#', $form_state['values']['scan'])) {
    $studentnumber = $form_state['values']['scan'];
  }
  // campus card number, direct input
  elseif (preg_match('#([0-9]{9})#', $form_state['values']['scan'])) {
    $cardnumber = $form_state['values']['scan'];
  }
  else {
    form_set_error('scan', 'The student number entered could not be parsed. Please check your input');
    return;
  }

  if ($studentnumber !== NULL) {
    $result = db_query('SELECT uid FROM {svid_leden} WHERE studentnumber = %d', $studentnumber);
    if (($form_state['values']['uid'] = db_result($result)) === FALSE) {
      form_set_error('scan', t('The student number entered could not be matched to a member. Please use the campus card instead.'));
    }
  }
  else {
    $form_state['values']['uid'] = variable_get('aselect_fallback_uid', 0);
    $form_state['values']['cardnumber'] = $cardnumber;
  }
}

function iof_tickets_customer_lookup_form_submit($form, &$form_state) {
  if ($form_state['values']['uid'] === variable_get('aselect_fallback_uid', 0)) {
    drupal_goto(sprintf('admin/store/iof-tickets/view/%d/%d', $form_state['values']['uid'], $form_state['values']['cardnumber']));
  }
  else {
    drupal_goto('admin/store/iof-tickets/view/' . $form_state['values']['uid']);
  }
}

/**
 * Retreive the orders for the given user.
 *
 * Also provide a form to add a custom order to the given user.
 */
function iof_tickets_customer_lookup_form_results($customer) {
  $output = drupal_get_form('iof_tickets_customer_lookup_form', $customer, TRUE);

  $output .= drupal_get_form('iof_tickets_order_list', $customer);

  $output .= drupal_get_form('iof_tickets_order_form', $customer);

  return $output;
}

function theme_iof_tickets_order_create_fieldset($form) {
  $fields = array('default_ticket', 'intro_ticket', 'tokens');
  $header = array(
    t('Product'),
    t('Price'),
    t('Count'),
  );
  $rows = array();
  foreach ($fields as $field) {
    $rows[] = array(
      drupal_render($form[$field]['title']),
      drupal_render($form[$field]['price']),
      drupal_render($form[$field][$field.'_qty']),
    );
  }

  $output = theme_table($header, $rows);
  $output .= drupal_render($form);

  return $output;
}

function iof_tickets_order_form(&$form_state, $customer, $inline = TRUE) {
  $default_ticket_nid = variable_get('iof_tickets_default_ticket_nid', 0);
  $intro_ticket_nid = variable_get('iof_tickets_intro_ticket_nid', 0);
  $tokens_nid = variable_get('iof_tickets_tokens_nid', 0);

  if ($default_ticket_nid === 0 || $intro_ticket_nid == 0 || $tokens_nid === 0) {
    drupal_set_message(t('You need to <a href="@config-link">configure</a> this module before using this functionality.', array('@config-link' => url('admin/store/settings/iof-tickets'))), 'warning');
    return array();
  }

  $default_ticket = node_load($default_ticket_nid);
  $intro_ticket = node_load($intro_ticket_nid);
  $tokens = node_load($tokens_nid);

  $fieldset = array();

  $fieldset = array(
    'customer' => array(
      '#type' => 'value',
      '#value' => $customer,
    ),
  );

  /**
   * If uc_price_per_role module is available, run the following code.
   *
   * This code is an adaptation of uc_price_per_role_find_price(), fitted
   * for use with a given customer, instead of the current user.
   */
  if (module_exists('uc_price_per_role')) {
    $prices = $default_ticket->role_prices;
    $default_ticket_price = $default_ticket->roleindependent_sell_price;

    $enabled = variable_get('uc_price_per_role_enabled', array());
    $weights = variable_get('uc_price_per_role_weights', array());
    asort($weights);

    foreach ($weights as $rid => $weight) {
      if (isset($customer->roles[$rid]) && $enabled[$rid] && isset($prices[$rid])) {
        $default_ticket_price = $prices[$rid];
      }
    }
  }
  else {
    $default_ticket_price = $default_ticket->sell_price;
  }

  $fieldset['default_ticket']['title']['#value'] = l(node_page_title($default_ticket), 'node/' . $default_ticket_nid);
  $fieldset['default_ticket']['price']['#value'] = uc_currency_format($default_ticket_price);
  $fieldset['default_ticket']['default_ticket_qty'] = array(
    '#type' => 'textfield',
    '#size' => 5,
  );

  $fieldset['intro_ticket']['title']['#value'] = l(node_page_title($intro_ticket), 'node/' . $intro_ticket_nid);
  $fieldset['intro_ticket']['price']['#value'] = uc_currency_format($intro_ticket->sell_price);
  $fieldset['intro_ticket']['intro_ticket_qty'] = array(
    '#type' => 'textfield',
    '#size' => 5,
  );

  $fieldset['tokens']['title']['#value'] = l(node_page_title($tokens), 'node/' . $tokens_nid);
  $fieldset['tokens']['price']['#value'] = uc_currency_format($tokens->sell_price);
  $fieldset['tokens']['tokens_qty'] = array(
    '#type' => 'textfield',
    '#size' => 5,
  );

  $fieldset['submit'] = array(
    '#type' => 'submit',
    '#value' => t('Create'),
  );

  $form = array(
    'order' => array(
      '#type' => 'fieldset',
      '#title' => t('Create order'),
      '#theme' => 'iof_tickets_order_create_fieldset',
      '#collapsible' => TRUE,
      '#collapsed' => FALSE,
    )
  );

  $form['order'] += $fieldset;

  return ($inline) ? $form : $fieldset;
}

function iof_tickets_order_form_submit($form, &$form_state) {  
  $default_ticket_nid = variable_get('iof_tickets_default_ticket_nid', 0);
  $intro_ticket_nid = variable_get('iof_tickets_intro_ticket_nid', 0);
  $tokens_nid = variable_get('iof_tickets_tokens_nid', 0);

  $default_ticket = node_load($default_ticket_nid);
  $intro_ticket = node_load($intro_ticket_nid);
  $tokens = node_load($tokens_nid);

  $customer = $form_state['values']['customer'];

  if (module_exists('uc_price_per_role')) {
    $prices = $default_ticket->role_prices;
    $default_ticket_price = $default_ticket->roleindependent_sell_price;

    $enabled = variable_get('uc_price_per_role_enabled', array());
    $weights = variable_get('uc_price_per_role_weights', array());
    asort($weights);

    foreach ($weights as $rid => $weight) {
      if (isset($customer->roles[$rid]) && $enabled[$rid] && isset($prices[$rid])) {
        $default_ticket_price = $prices[$rid];
      }
    }
  }
  else {
    $default_ticket_price = $default_ticket->sell_price;
  }
  
  $order = uc_order_new($customer->uid, 'processing');

  $order->payment_method = 'none';
  $order->customer_cardnumber = $customer->cardnumber;
  $order->delivery_method = 'pickup';

  if ($customer->uid !== variable_get('aselect_fallback_uid', 0)) {
    $order->delivery_first_name = $customer->firstname;
    $order->delivery_last_name = $customer->lastname;
    $order->delivery_street1 = $customer->address;
    $order->delivery_postal_code = $customer->zipcode;
    $order->delivery_city = $customer->city;
  }

  $order->products = array();

  // Default ticket
  if ($form_state['values']['default_ticket_qty'] > 0) {
    $order->products[] = (object) array(
      'nid' => $default_ticket_nid,
      'title' => $default_ticket->title,
      'model' => $default_ticket->model,
      'qty' => $form_state['values']['default_ticket_qty'],
      'price' => $default_ticket_price,
    );
  }

  // Introduction ticket
  if ($form_state['values']['intro_ticket_qty'] > 0) {
    $order->products[] = (object) array(
      'nid' => $intro_ticket_nid,
      'title' => $intro_ticket->title,
      'model' => $intro_ticket->model,
      'qty' => $form_state['values']['intro_ticket_qty'],
      'price' => $intro_ticket->sell_price,
    );
  }

  // Tokens
  if ($form_state['values']['tokens_qty'] > 0) {
    $order->products[] = (object) array(
      'nid' => $tokens_nid,
      'title' => $tokens->title,
      'model' => $tokens->model,
      'qty' => $form_state['values']['tokens_qty'],
      'price' => $tokens->sell_price,
    );
  }

  uc_order_save($order);

  uc_order_update_status($order->order_id, 'pickup');

  drupal_set_message(t('Order created. Proceed with payment.'));
}

/**
 * List of orders for a given customer.
 */
function iof_tickets_order_list(&$form_state, $customer) {
  $default_ticket_nid = variable_get('iof_tickets_default_ticket_nid', 0);
  $intro_ticket_nid = variable_get('iof_tickets_intro_ticket_nid', 0);
  $tokens_nid = variable_get('iof_tickets_tokens_nid', 0);

  if ($default_ticket_nid === 0 || $intro_ticket_nid == 0 || $tokens_nid === 0) {
    drupal_set_message(t('You need to <a href="@config-link">configure</a> this module before using this functionality.', array('@config-link' => url('admin/store/settings/iof-tickets'))), 'warning');
    return array();
  }

  $default_ticket = node_load($default_ticket_nid);
  $intro_ticket = node_load($intro_ticket_nid);
  $tokens = node_load($tokens_nid);

  $form = array();
  $container = array();

  $form['customer'] = array(
    '#type' => 'value',
    '#value' => $customer,
  );

  $default_ticket_total = 0;
  $intro_ticket_total = 0;
  $tokens_total = 0;

  $orders = array();

  if (isset($customer->orders) && is_array($customer->orders)) {
    foreach ($customer->orders as $order_id) {
      $default_ticket_qty = $intro_ticket_qty = $tokens_qty = 0;
      $order = uc_order_load($order_id);

      if ($order->order_status == 'in_checkout') {
        continue;
      }

      // Lookup product quantities.
      foreach ($order->products as $product) {
        switch($product->nid) {
          case $default_ticket_nid:
            $default_ticket_qty = $product->qty;
            $default_ticket_total += $default_ticket_qty;
            break;
          case $intro_ticket_nid:
            $intro_ticket_qty = $product->qty;
            $intro_ticket_total += $intro_ticket_qty;
            break;
          case $tokens_nid:
            $tokens_qty = $product->qty;
            $tokens_total += $tokens_qty;
            break;
        }
      }

      // If order is waiting for pickup, show. Otherwise, discard.
      if ($order->delivery_method != 'pickup' || $order->order_status == 'picked_up') {
        continue;
      }

      // Place order in form.
      $orders[$order_id] = '';
      $form['default_ticket_qty'][$order_id] = array('#value' => $default_ticket_qty);
      $form['intro_ticket_qty'][$order_id] = array('#value' => $intro_ticket_qty);
      $form['tokens_qty'][$order_id] = array('#value' => $tokens_qty);
      $form['order_total'][$order_id] = array('#value' => uc_currency_format($order->order_total));
      $form['payment_received'][$order_id] = array('#value' => ($order->payment_method == 'none') ? t('No') : t('Yes'));
    }
  }

  $form['order_complete'] = array(
    '#type' => 'checkboxes',
    '#options' => $orders,
  );

  $form['submit'] = array(
    '#type' => 'submit',
    '#value' => t('Order picked up'),
  );

  $container['pending_orders'] = array(
    '#type' => 'fieldset',
    '#title' => t('Orders waiting for pickup'),
    '#theme' => 'iof_tickets_order_list_fieldset',
    '#weight' => -40,
  );

  $container['pending_orders'] += $form;

  $container['total_orders'] = array(
    '#type' => 'fieldset',
    '#title' => t('Total orders from customer.'),
    '#weight' => -50,
  );
  $container['total_orders']['table'] = array(
    '#value' => theme('table',
      array(
        $default_ticket->title,
        $intro_ticket->title,
        $tokens->title,
      ),
      array(array(
        $default_ticket_total,
        $intro_ticket_total,
        $tokens_total,
      ))
    ),
  );

  return $container;
}

function iof_tickets_order_list_validate($form, &$form_state) {
  $form_state['values']['order_complete'] = array_filter($form_state['values']['order_complete']);
  if (count($form_state['values']['order_complete']) == 0) {
    form_set_error('', t('No orders selected'));
  }
}

function iof_tickets_order_list_submit($form, &$form_state) {
  foreach ($form_state['values']['order_complete'] as $order_id) {
    if (uc_order_update_status($order_id, 'picked_up') === FALSE) {
      drupal_set_message(t('Could not update order %order_id. Status update was refused.', array('%order_id' => $order_id)), 'error');
    }
  }

  drupal_set_message(t('Updated order queue.'));
}

/**
 * Theme the list of orders for a selected customer.
 *
 * @param $form The unthemed Forms API data.
 * @return Themed output of the order list form.
 */
function theme_iof_tickets_order_list_fieldset($form) {
  $default_ticket_nid = variable_get('iof_tickets_default_ticket_nid', 0);
  $intro_ticket_nid = variable_get('iof_tickets_intro_ticket_nid', 0);
  $tokens_nid = variable_get('iof_tickets_tokens_nid', 0);

  if ($default_ticket_nid === 0 || $intro_ticket_nid == 0 || $tokens_nid === 0) {
    drupal_set_message(t('You need to <a href="@config-link">configure</a> this module before using this functionality.', array('@config-link' => url('admin/store/settings/iof-tickets'))), 'warning');
    return array();
  }

  $default_ticket = node_load($default_ticket_nid);
  $intro_ticket = node_load($intro_ticket_nid);
  $tokens = node_load($tokens_nid);

  $header = array(
    theme('table_select_header_cell'),
    $default_ticket->title,
    $intro_ticket->title,
    $tokens->title,
    t('Total'),
    t('Betaald'),
  );

  $rows = array();

  if (isset($form['order_total']) && is_array($form['order_total'])) {
    foreach(element_children($form['order_total']) as $order_id) {
      $rows[] = array(
        drupal_render($form['order_complete'][$order_id]),
        drupal_render($form['default_ticket_qty'][$order_id]),
        drupal_render($form['intro_ticket_qty'][$order_id]),
        drupal_render($form['tokens_qty'][$order_id]),
        drupal_render($form['order_total'][$order_id]),
        drupal_render($form['payment_received'][$order_id]),
      );
    }
  }
  else {
    $rows[] = array(array('data' => t('No orders found.'), 'colspan' => 6));
  }

  $output .= theme('table', $header, $rows);
  
  $output .= drupal_render($form);

  return $output;
}

/**
 * Ticket export form.
 *
 * Export a list of addresses and the orders connected to them.
 */
function iof_tickets_order_export_form() {
  $form = array();
  $form['export_type'] = array(
    '#title' => t('Scope'),
    '#type' => 'radios',
    '#options' => array(
      'all' => t('All orders'),
      'online' => t('Orders purchased online'),
      'mail' => t('Shippable orders'),
      'picking' => t('Orders waiting to be shipped'),
      'mailed' => t('Shipped orders'),
    ),
  );

  $form['update_status'] = array(
    '#title' => t('Set exported orders to shipped.'),
    '#type' => 'checkbox',
    '#default_value' => FALSE,
    '#description' => t('If set, exported orders that are marked as waiting to be shipped will be updated to shipped orders.'),
  );

  $form['export'] = array(
    '#type' => 'submit',
    '#value' => t('Export'),
  );

  return $form;
}

function iof_tickets_order_export_form_submit($form, &$form_state) {
  if ($form_state['values']['update_status']) {
    $form_state['values']['export_type'] .= '-update';
  }

  drupal_goto('admin/store/iof-tickets/export/' . $form_state['values']['export_type']);
}

function iof_tickets_order_export($action) {
  $default_ticket_nid = variable_get('iof_tickets_default_ticket_nid', 0);
  $intro_ticket_nid = variable_get('iof_tickets_intro_ticket_nid', 0);
  $tokens_nid = variable_get('iof_tickets_tokens_nid', 0);

  if ($default_ticket_nid === 0 || $intro_ticket_nid == 0 || $tokens_nid === 0) {
    drupal_set_message(t('You need to <a href="@config-link">configure</a> this module before using this functionality.', array('@config-link' => url('admin/store/settings/iof-tickets'))), 'warning');
    drupal_goto('admin/store/iof-tickets/export');
  }

  $default_ticket = node_load($default_ticket_nid);
  $intro_ticket = node_load($intro_ticket_nid);
  $tokens = node_load($tokens_nid);

  $header = array(
    t('Order ID'),
    t('Order date'),
    t('Order status'),
    t('First name'),
    t('Last name'),
    t('Address'),
    t('Postal code'),
    t('City'),
    t('Delivery method'),
    t('Number of tickets (member)'),
    t('Number of tickets (intro)'),
    t('Number of tokens'),
    t('Order total'),
    t('Payment method'),
  );
  $rows = array();

  $actions = explode('-', $action);
  $scope = $actions[0];
  $update_status = isset($actions[1]);

  switch ($scope) {
    case 'all':
      $query = 'SELECT order_id
                FROM {uc_orders}';
      break;

    case 'online':
      $query = "SELECT order_id
                FROM {uc_orders}
                WHERE payment_method = 'ideal_lite_payment'";
      break;

    case 'mail':
      $query = "SELECT order_id
                FROM {uc_order_iof_tickets}
                WHERE delivery_method = 'mail'";
      break;

    case 'picking':
      $query = "SELECT o.order_id
                FROM {uc_orders} o, {uc_order_iof_tickets} t
                WHERE o.order_id = t.order_id
                  AND t.delivery_method = 'mail'
                  AND o.order_status = 'processing'";
      break;

    case 'mailed':
      $query = "SELECT o.order_id
                FROM {uc_orders} o, {uc_order_iof_tickets} t
                WHERE o.order_id = t.order_id
                  AND t.delivery_method = 'mail'
                  AND o.order_status = 'shipped'";
      break;
    
    default:
      drupal_set_message(t('Unknown scope.'), 'warning');
      drupal_goto('admin/store/iof-tickets/export');
      break;
  }

  $result = db_query($query);
  if ($result === FALSE) {
    drupal_set_message(t('Could not load list of order_id\'s'), 'error');
    drupal_goto('admin/store/iof-tickets/export');
  }

  while (($order = db_fetch_object($result)) !== FALSE) {
    $default_ticket_qty = $intro_ticket_qty = $tokens_qty = 0;
    $order = uc_order_load($order->order_id);

    // Lookup product quantities.
    foreach ($order->products as $product) {
      switch($product->nid) {
        case $default_ticket_nid:
          $default_ticket_qty += $product->qty;
          break;
        case $intro_ticket_nid:
          $intro_ticket_qty += $product->qty;
          break;
        case $tokens_nid:
          $tokens_qty = $product->qty;
          break;
      }
    }

    $rows[] = array(
      $order->order_id,
      format_date($order->created, 'small'),
      $order->order_status,
      // Delivery information.
      $order->delivery_first_name,
      $order->delivery_last_name,
      $order->delivery_street1,
      $order->delivery_postal_code,
      $order->delivery_city,
      $order->delivery_method,
      // Order information
      $default_ticket_qty,
      $intro_ticket_qty,
      $tokens_qty,
      $order->order_total,
      $order->payment_method,
    );

    if ($update_status) {
      uc_order_update_status($order->order_id, 'shipped');
    }
  }

  if (empty($rows)) {
    drupal_set_message(t('Could not find orders matching the selected criteria'));
    drupal_goto('admin/store/iof-tickets/export');
  }

  $filename = 'order-list-' . $scope . '.csv';

  header('Content-Type: text/plain');
  header('Content-Disposition: attachment; filename=' . $filename);
  echo str_putcsv($header, ';');
  foreach ($rows as $line) {
    echo str_putcsv($line, ';');
  }
  exit();
}

/**
 * Utility function for placing array data in CSV format.
 */
if (!function_exists('str_putcsv')) {
  function str_putcsv($input, $delimiter = ',', $enclosure = '"') {
    $fp = fopen('php://temp', 'w+');
    fputcsv($fp, $input, $delimiter, $enclosure);
    rewind($fp);
    $data = fread($fp, 1048576);
    fclose($fp);
    return $data;
  }
}
<?php
// $Id$

function iof_tickets_settings_form() {
  $result = db_query('SELECT t.nid, n.title FROM {term_node} t, {uc_products} p, {node} n WHERE t.tid = 1 AND t.nid = p.nid AND p.nid = n.nid');
  if ($result === false) {
    drupal_set_message(t('Could not retrieve available products from the database'), 'error');
  }

  $products = array(
    0 => t('select product')
  );
  while (($product = db_fetch_array($result)) !== FALSE) {
    $products[$product['nid']] = $product['title'];
  }
  
  $form = array();
  $form['products'] = array(
    '#type' => 'fieldset',
    '#title' => t('Product mapping'),
  );

  $form['products']['iof_tickets_default_ticket_nid'] = array(
    '#type' => 'select',
    '#title' => t('Default ticket'),
    '#options' => $products,
    '#default_value' => variable_get('iof_tickets_default_ticket_nid', 0),
  );

  $form['products']['iof_tickets_intro_ticket_nid'] = array(
    '#type' => 'select',
    '#title' => t('Introduction ticket'),
    '#options' => $products,
    '#default_value' => variable_get('iof_tickets_intro_ticket_nid', 0),
  );

  $form['products']['iof_tickets_tokens_nid'] = array(
    '#type' => 'select',
    '#title' => t('Tokens'),
    '#options' => $products,
    '#default_value' => variable_get('iof_tickets_tokens_nid', 0),
  );

  return system_settings_form($form);
}
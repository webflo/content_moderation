<?php

// $Id$

/**
 * @file
 * Copyright (c) 2010 Impressive.media
 * Author: Eugen Mayer
 */


/**
 * General form to change the state of a revision
 * Currently we only support changing from one step to the other
 */
function content_moderation_change_state_form(&$form, &$form_state, $node) {
  // @TODO this has to respect user perms
  $states = content_moderation_transitions_map($node->content_moderation['current_state']);
  // Build the array of states that this node can use
  $options = array();
  foreach ($states as $state) {
    $options[$state] = $state;
  }

  $form = array();
  $form['node'] = array (
    '#type' => 'value',
    '#value' => $node
  );

  $form['description'] = array (
    '#type' => 'fieldset',
    '#title' => t('Change this revision\'s state'),
  );

  $form['description']['state_name'] = array (
    '#type' => 'select',
    '#title' => t('New state'),
    '#description' => t('Current state: @curstate. Choose the next workflow state this revision should be set to.', 
      array('@curstate' => $node->content_moderation['current_state'])),
    '#options' => $options,
  );

  $form['description']['comment'] = array (
    '#type' => 'textarea',
    '#title' => t('Comment'),
    '#description' => t('If you wish, add an optional comment. This will be shown in the history')
  );

  $form['buttons']['submit'] = array (
    '#type'=>'submit',
    '#value'=>t('Save')
  );
  $form['buttons']['cancel']['#type']   = 'markup';
  $form['buttons']['cancel']['#weight'] = 2000;
  $form['buttons']['cancel']['#value']  = l(t('Cancel'), "node/{$node->nid}/revisions/{$node->vid}/view", array( 'attributes' => array( 'class' => 'form-submit form-submit-cancel')));
  return $form;
}


/**
 * Implementation of hook_form_validate
 *
 * Validate if the user is providing a valid next state and is allowed to change state
 * of the provided node
 */
function content_moderation_change_state_form_validate1($form_id, $form_values) {
  global $user;

  // Is this user allowed to change the state of this revision?
  if(!_content_moderation_statechange_allowed($form_values['values']['node']->vid, $form_values['values']['nextstate'])) {
      form_set_error('nextstate',t('You are not allowed to change to that state!'));
  }
}


/**
 * Implementation of hook_form_submit
 */
function content_moderation_change_state_form_submit($form, &$form_state) {
  global $user;

  $node = $form_state['values']['node'];
 
  // Create a record that can be saved
  $record = array (
    'nid' => $node->nid,
    'vid' => $node->vid,
    'uid' => $user->uid,
    'old_state_name' => $node->content_moderation['current_state'],
    'state_name' => $form_state['values']['state_name'],
    'stamp' => time(),
    'comment' => $form_state['values']['comment']
  );

  // Update the node's state
  content_moderation_node_state_update($record);
  // Save a record of this transition
  content_moderation_node_history_save($record);

 
  // If the state is live, change the current live version.
  if ($form_state['values']['state_name'] == content_moderation_live_state()) {
    content_moderation_node_set_live($node);
    drupal_set_message(t("This revision !vid has now been published as the new live version.", array('!vid' => $node->vid)));
  }

  drupal_set_message(t('The revisions state has been successfully updated to @state',array('@state' => $form_state['values']['state_name'])));

  // Update the node with the new HID for any hooks that get fired
  $node->content_moderation[$record['hid']] = $record;
  $node->content_moderation['live'] = $record;

  // As we log everything this is the proper place to fire any events for trigger
  if (module_exists('trigger')) {
    module_invoke_all('content_moderation', "{$node->type}_{$node->content_moderation['current_state']}_{$form_state['values']['state_name']}", $user);
  }
  if (module_exists('rules')) {
    rules_invoke_event("{$node->type}_{$node->content_moderation['current_state']}_{$form_state['values']['state_name']}}", $node);
  }

  if ($form_state['values']['state_name'] == content_moderation_live_state()) {
    drupal_goto('node/' . $node->nid);
  }
  else {
    drupal_goto("node/{$node->nid}/revisions/{$node->vid}/view");
  }
}


/**
 * Utility function to save a new history entry for a node
 */
function content_moderation_node_history_save(&$record) {
  // Save the entry
  drupal_write_record('content_moderation_node_history', $record);
}

/**
 * Update state of a node
 */
function content_moderation_node_state_update(&$record) {
  $keys = array();
  // Check if we have an entry allready. Update if needed.
  $result = db_query("SELECT vid FROM {content_moderation_revision_state} WHERE vid = :vid", array(':vid' => $record['vid']))->fetchField();
  $keys = NULL;
  if ($result) {
    $keys = array('vid');
  }
  drupal_write_record('content_moderation_revision_state', $record, $keys);
}


/**
 * Set this VID to the live version
 */
function content_moderation_node_set_live($node) {
  $revision = _content_moderation_revision_infos($vid);
  dsm($revision);
  // Save the vid in the node table and update the title.
  db_query("UPDATE {node} SET vid = %d, status = 1, changed='%s', title='%s' WHERE nid = %d", $vid, $revision->timestamp, $revision->title, $nid);
  if(module_exists('pathauto'))  {
    // recreate node-path_alias
    $node = node_load($nid, NULL, true);
    pathauto_node_update_alias($node,'update');
  }

  // Reset all current states of revisisions of this node, which are live
  // as we can have more then one live revision.
  // We are not resetting the history though, because it should be visible
  // which node has been live, approved by who and so forth.
  db_query("UPDATE {content_moderation_revision_state} SET state = 'none' WHERE nid = %d AND vid<>%d AND state='live'", $nid,$vid);
}












/*
 * Generally check, whether the node-type can be moderated at all
 * and whether the user is allowed to change into that state for that
 * node type.
 */
function _content_moderation_statechange_allowed($vid,$state = NULL) {
  global $user;
  $nid = _content_moderation_get_nid($vid);
  $node = node_load($nid,$vid);

  // Is this content even in moderatation?
  if(! content_moderation_node_type_moderated($node->type)) {
    drupal_set_message(t('this content type (!type) is not under moderation', array('!type' => $node->type)),'error');
    return false;
  }

  if(variable_get('content_moderation_moderate_latest_only', 1) === 1) {
    $revisions = _content_moderation_get_latest_revisions($nid,1);
    // Do not allow state changing of revisions, which are not the latest

    if(!is_array($revisions) || $revisions[0]->vid != $vid) {
      return false;
    }
  }

  // User must have write access to do anything state related
  if(module_exists('impressive_media_icombinded_access')) {
    if(impressive_media_icombined_access_accesscheck('update', $node) === false) {
      return false;
    }
  }
  else {
    if(node_access('update', $node) === false) {
      return false;
    }
  }

  // Does the user has rights to change to any allowed next states?
  $curstate = _content_moderation_current_state($vid);
  if($state != NULL) {
    return _content_moderation_state_allowed($user, $curstate, $state, $node->type);
  }
  else if(count(_content_moderation_next_states($curstate, $node->type, $user)) > 0) {
    return true;
  }
  //else
  return false;
}

/*
 * Get the current state of a revision *
 */
function _content_moderation_current_state($vid) {
  $result = db_query("SELECT state_name FROM {content_moderation_revision_state} WHERE vid = :vid", array(':vid' => $vid))->fetchField();
  if ($result) {
    return $result;
  }
  // return default
  return 'none';
}


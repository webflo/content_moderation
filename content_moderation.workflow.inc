<?php
// $Id$

# Copyright (c) 2010 Impressive.media
# Author: Eugen Mayer

/*
 * General form to change the state of a revision
 * Currently we only support changing from one step to the other
 */
function content_moderation_change_state_form($form, &$form_state, $vid) {
  global $user;

  $curstate = _content_moderation_current_state($vid);
  $nid = _content_moderation_get_nid($vid);
  $node = node_load($nid, $vid);
  // Get all states we can switch to respecting the user rights and the transition
  // table
  if (! $states = _content_moderation_next_states($curstate, $node->type, $user)) {
    drupal_set_message(t('Sorry, there are no new moderation states for the %state state.
      Perhaps you need to !create?', array(
        '%state' => $curstate,
        '!create' => l(t('create another'), 'admin/config/content_moderation/transitions'))),
      'error'
    );
    return;
  }
   
  $form = array();
  $form['#attributes'] = array('enctype' => "multipart/form-data");
  $form['user'] = array(
    '#type' => 'value',
    '#value' => $user
  );
  $form['node'] = array(
    '#type' => 'value',
    '#value' => $node
  );

  $form['oldstate'] = array(
    '#type' => 'value',
    '#default_value' => $curstate
  );

  $form['description'] = array(
    '#type' => 'fieldset',
    '#title' => t('Changing state of revision'),
  );

  $form['description']['nextstate'] = array(
    '#type' => 'select',
    '#title' => t('New state'),
    '#prefix' => t('Current state: @curstate', array('@curstate' => $curstate)),
    '#description' => t('Choose the next workflow state this revision should be set to.'),
    '#options' => $states,
  );

  $form['description']['comment'] = array(
    '#type' => 'textarea',
    '#title' => t('Comment'),
    '#description' => t('If you wish, add an optional comment. This will be shown in the history')
  );

  $form['buttons']['submit']            = array('#type' => 'submit', '#value' => t('Save'));
  $form['buttons']['cancel']['#type']   = 'markup';
  $form['buttons']['cancel']['#weight'] = 2000;
  $form['buttons']['cancel']['#value']  = l(t('Cancel'), "node/$nid/revisions/$vid/view", array('attributes' => array('class' => 'form-submit form-submit-cancel')));
  return $form;
}

/*
 * Implementation of hook_form_validate
 * Validate if the user is providing a valid next state and is allowed to change state
 * of the provided node
 */
function content_moderation_change_state_form_validate($form_id, $form_values) {
  global $user;

  // Is this user allowed to change the state of this revision?
  if (!_content_moderation_statechange_allowed($form_values['values']['node']->vid, $form_values['values']['nextstate'])) {
    form_set_error('nextstate', t('You are not allowed to change to that state!'));
  }
}

/*
 * Implementation of hook_form_submit
 */
function content_moderation_change_state_form_submit($form, &$form_state) {
  global $user;

  $node = $form_state['values']['node'];
  $curstate = $form_state['values']['oldstate'];
  $nextstate = $form_state['values']['nextstate'];
  $comment = $form_state['values']['comment'];

  // Change the revision state.
  _content_moderation_update_revision_state($node->vid, $node->nid, $nextstate);

  // If the state is live, change the current live version.
  // TODO: the "final" state maybe should not be hardcoded later.
  if ($nextstate == 'live') {
    _content_moderation_set_live($node->vid, $node->nid);
    drupal_set_message(t("This revision !vid has now been published as the new live version.", array('!vid' => $node->vid)));
  }

  // History entry.
  _content_moderation_save_history($node, $curstate, $nextstate, $user->uid, $comment);
  drupal_set_message(t('The revisions state has been successfully updated to @state', array('@state' => $nextstate)));

  if ($nextstate == 'live') {
    drupal_goto('node/' . $node->nid);
  }
  else {
    drupal_goto("node/{$node->nid}/revisions/{$node->vid}/view");
  }
}

/*
 * Update state.
 */
function _content_moderation_update_revision_state($vid, $nid, $state) {
  $revison_state = array(
    'nid' => $nid,
    'vid' => $vid,
    'state' => $state,
  );
  $keys = array();
  // Check if we have an entry allready. Update if needed.
  $result = db_query("SELECT vid FROM {content_moderation_revision_state} WHERE vid = :vid", array(':vid' => $vid))->fetchField();
  if ($result) {
    $keys = array('vid');
  }
  drupal_write_record('content_moderation_revision_state', $revison_state, $keys);
}


/*
 * Make the given vid the new live version.
 */
function _content_moderation_set_live($vid, $nid) {
  $revision = _content_moderation_revision_infos($vid);
  // Save the vid in the node table and update the title.
  $record = array(
    'vid' => $vid,
    'changed' => $revision->timestamp,
    'status' => 1,
    'title' => $revision->title,
    'nid' => $nid
  );
  drupal_write_record('nide', $record, 'nid');
  
  if (module_exists('pathauto')) {
    // recreate node-path_alias
    $node = node_load($nid, NULL, TRUE);
    pathauto_node_update_alias($node, 'update');
  }

  // Reset all current states of revisisions of this node, which are live
  // as we can have more then one live revision.
  // We are not resetting the history though, because it should be visible
  // which node has been live, approved by who and so forth.
  $update = array(
    'state' => _content_moderation_none_state(),
    'nid' => $nid,
    'vid' => $vid,
  );
  drupal_write_record('content_moderation_revision_state', $update, array('nid', 'vid'));
 }

/*
 * Save an entry in the content moderation history
 */
function _content_moderation_save_history($node, $curstate, $nextstate, $uid, $comment) {
  $history = array(
    'nid' => $node->nid,
    'vid' => $node->vid,
    'uid' => $uid,
    'old_state_name' => $curstate,
    'state_name' => $nextstate,
    'stamp' => REQUEST_TIME,
    'comment' => $comment
  );

  drupal_write_record('content_moderation_node_history', $history, array());
  // As we log everything this is the proper place to fire any events for trigger
  if (module_exists('trigger')) {
    module_invoke_all('content_moderation', "{$node->type}_{$curstate}_{$nextstate}", $user);
  }
  if (module_exists('rules')) {
    rules_invoke_event("{$node->type}_{$curstate}_{$nextstate}", $node);
  }
}

/*
 * Gets the history of a node or a revision, depending on the key.
 */
function _content_moderation_get_history($key, $tostate = NULL) {
  if ($tostate != NULL) {
    $results = db_query("SELECT * FROM {content_moderation_node_history}
      WHERE {$key['name']} = :value
        AND state_name = :state
      ORDER BY stamp DESC",
      array(
        ':value' => $key['value'],
        ':state' => $tostate
      )
    );
  }
  else {
    $results = db_query("SELECT * FROM {content_moderation_node_history}
      WHERE {$key['name']} = :value
      ORDER BY stamp DESC",
      array(
        ':value' => $key['value']
      )
    );
  }

  $history = array();
  foreach ($results as $entry) {
    $history[] = $entry;
  }
  return $history;
}

/*
 * Checks all transition from the current revision state to any valid other state.
 * Optional also filters any next-states which this user is not allowed to change to
 *
 * @curstate: current state of the revision
 * @node_type: the node type is used to look for valid transition for this specific node type
 * @user: if set, all resulting states are filter to states allowed for this user
 */
function _content_moderation_next_states($curstate, $node_type = 'all', $user = NULL) {
  if ($curstate == "") {
    $curstate = _content_moderation_none_state();
  }
  $results = db_query("SELECT to_name AS state
    FROM {content_moderation_transition}
    WHERE from_name = :from_name
      AND (
        ntype = :ntype
        OR ntype = 'all'
      )",
    array(
      ':from_name' => $curstate,
      ':ntype' => $node_type
    )
  );

  $states = array();
  foreach($results as $state) {
    $states[$state->state] = t($state->state);
  }

  if ($user != NULL) {
    foreach ($states as $state_name => $value) {
      if (!_content_moderation_state_allowed($user, $curstate, $state_name, $node_type)) {
        unset($states[$state_name]);
      }
    }
  }

  return $states;
}

/*
 * Get all states possible
 */
function _content_moderation_states() {
  $results = db_query('SELECT * FROM {content_moderation_states}');
  //TODO: cache this
  $states = array();
  foreach ($results as $state) {
    $states[] = $state->name;
  }
  return $states;
}

/*
 * Generally check, whether the node-type can be moderated at all
 * and whether the user is allowed to change into that state for that
 * node type.
 */
function _content_moderation_statechange_allowed($vid, $state = NULL) {
  global $user;
  $nid = _content_moderation_get_nid($vid);
  $node = node_load($nid, $vid);

  // Is this content even in moderatation?
  if (!_content_moderation_moderate_node_type($node->type)) {
    drupal_set_message(t('this content type (!type) is not under moderation', array('!type' => $node->type)), 'error');
    return FALSE;
  }


  if (variable_get('content_moderation_moderate_latest_only', 1)) {
    // Are there any revisions for this node?
    if (! $revisions = _content_moderation_get_latest_revisions($nid, 1)) {
      return FALSE;
    }
    // Do not allow state changing of revisions, which are not the latest
    if ($revisions[0]->vid != $vid) {
      return FALSE;
    }
  }

  // User must have write access to do anything state related
  // @TODO this needs to be abstracted to a hook
  if (module_exists('impressive_media_icombinded_access')) {
    if (impressive_media_icombined_access_accesscheck('update', $node) === FALSE) {
      return FALSE;
    }
  }
  else {
    if (node_access('update', $node) === FALSE) {
      return FALSE;
    }
  }

  // Does the user has rights to change to any allowed next states?
  $curstate = _content_moderation_current_state($vid);
  if ($state != NULL) {

    return _content_moderation_state_allowed($user, $curstate, $state, $node->type);
  }
  else if (count(_content_moderation_next_states($curstate, $node->type, $user)) > 0) {
    return TRUE;
  }

  //else
  return FALSE;
}

/*
 * Get the current state of a revision *
 */
function _content_moderation_current_state($vid) {
  $state = db_query("SELECT state FROM {content_moderation_revision_state} WHERE vid = :vid", array(':vid' => $vid))->fetchfield();
  if ($state) {
    return $state;
  }
  return _content_moderation_none_state();
}


<?php

// $Id$

/**
 * @file
 *
 * Provides node functions
 */


/**
 * Displays the current moderated version of the node
 *
 * @param $node
 *   Object, Drupal node object
 * @return html
 */
function content_moderation_node_current_view($node) {
  if (! content_moderation_node_is_current($node)) {
    // Load the current node
    $current_node = content_moderation_node_current_load($node);
    drupal_set_message(t('You are viewing the current moderated version of this
      !type. This is currently in state: !state.',
      array(
        '!type' => $node->type,
        '!state' => $current_node->content_moderation['current']->state
      )
    ));
    menu_set_active_item("node/{$current_node->nid}/moderation_history");
    return node_page_view($current_node);
  }
  // Node is current redirect and alert user
  drupal_set_message(t('This !type is the current version.', array('!type' => $node->type)));
  drupal_goto('node/' . $node->nid);
}



/**
 * Display a node's moderation history
 */
function content_moderation_node_history_view($node) {
  drupal_add_css(drupal_get_path('module', 'content_moderation') . '/css/content_moderation_history.css');
  drupal_set_title(t('!title History', array('!title' => $node->title)));

  // Get all of the node revisions including state changes
  $revisions = array();
  $result = db_query('SELECT r.vid, r.title, r.log, r.uid, n.vid AS current_vid, r.timestamp, u.name, cmnh.*
    FROM {node_revision} r
    LEFT JOIN {node} n ON n.vid = r.vid
    INNER JOIN {users} u ON u.uid = r.uid
    INNER JOIN {content_moderation_node_history} cmnh ON cmnh.vid = r.vid
    WHERE r.nid = :nid 
    ORDER BY r.vid DESC',
    array(':nid' => $node->nid));

  foreach ($result as $revision) {
    $revisions[$revision->vid] = $revision;
  }

  if ($revisions) {
    foreach ($revisions as $revision) {
      $row = array();
      $operations = array();
      extract((array) $revision);
      $text = '';
      $class = '';
      // Is this the live node?
      if ($vid == $node->content_moderation['live']->vid) {
        $text = t('Live revision');
        $class = 'revision-current';
      }
      if ($vid == $node->content_moderation['current']->vid) {
         $text =  t('Current revision');
        $class = 'revision-current';
      }
   
      $state_change = t('From !state to !to_state', array('!state' => $old_state_name, '!to_state' => $state_name));

      $row = array('data' => array(
        array('data' => $state_change, 'class' => $class),
        array('data' => l(format_date($timestamp, 'short'), "node/$node->nid/revisions/$vid/view"), 'class' => $class),
        array('data' => theme('username', array('account' => $revision)), 'class' => $class),
        array('data' => '<p class="revision-log">' . filter_xss($log) . '</p>', 'class' => $class),
        array('data' => $text, 'class' => $class)
        )
      );

      // @TODO need to figure out what to do with this
      if ($revert_permission) {
        $row['data'][] = l(t('revert'), "node/$node->nid/revisions/$vid/revert");
      }
      if ($delete_permission) {
        $row['data'][] =  l(t('delete'), "node/$node->nid/revisions/$vid/delete");
      }

      $rows[] = $row; 
    }
    $header = array(t('State change'), t('Date'), t('By'), t('Log'), t('Status'));
    return theme('table', array('header' => $header, 'rows' => $rows));
  }
}
<?php
// $Id$

// Copyright (c) 2010 Impressive.media
// Author: Eugen Mayer


/**
 * @file
 * Provides formating for the history block
 *
 * This nees some real cleanup to get the logic out
 * of the template. Much of this has been moved up to the
 * calling functions, but there is still more to be done.
 */

extract($arguments);

?>
<div id="content_moderation">
  <h4><?php print t('Live')?></h4>
  <?php if ($live != NULL) {
    $live_user = user_load($live->revision_uid);
    ?>
    <div class="info live_info"><?php print $live->vid .': ' . $live_link . $view_live_link . $compare ;?>

    <br/>
    <span class="details">&raquo; <?php print format_date($live->revision_timestamp, 'small')?> (<?php print $live_user->name ?>)</span>
    </div>
  <?php }  else { ?>
    <div class="info live_info"><?php print t('nothing approved yet');?></div>
  <?php } ?>


  <?php if ($live == NULL || $node->vid != $live->vid) { ?>
  <h4><?php print t('Viewing')?></h4>
  <div class="info">
    <label>
      <?php print t('Revision: !revision', array('!revision' => $current_rev_link)); ?>:
    </label>
    <?php
      print t('(State: !state)', array('!state' => $state));
      print $edit_state;
      isset($compare_with_live) ? print $compare_with_live : NULL;
      print $delete_current;
    ?>
    <br/>
    <span class="details">&raquo; <?php print format_date($node->revision_timestamp, 'small'); ?> (<?php print $node->name; ?>)</span>
    <span class="details state">&raquo; Status: <?php print t($state)?></span>
  </div>
  <?php }?>

  <?php if($revisions_list != "") { ?>
  <h4><?php print t('Pending')?></h4>
     <div class="info">
        <?php echo $revisions_list; ?>
     </div>
   <?php } ?>
</div>

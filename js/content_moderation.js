// $Id;

/**
 * @file
 * Provides javascript services
 */


(function ($) {

  // On page load either show or uncheck the moderation options
  var checked = $('#edit-node-options-revision').attr('checked');
  if (checked) {
    // We can show moderation items
    $('.form-item-content-moderation-default-state').show();
    $('.form-item-workflow-state').show();
  }
  else {
    // We can hide moderation items
    $('.form-item-content-moderation-default-state').hide().attr('checked', false);
    $('.form-item-workflow-state').hide();
  }


/**
 * Behavior to disable the "unflag" option if "flag" is not available.
 */
Drupal.behaviors.contentModeration = {};
Drupal.behaviors.contentModeration.attach = function(context) {
  $('#edit-node-options-revision').bind('click', function () {
    if ($(this).attr('checked')) {
      $('.form-item-content-moderation-default-state').show('slow');
      $('.form-item-workflow-state').show('slow');
    }
    else {
      // We can hide moderation items
      $('.form-item-content-moderation-default-state').hide('slow').attr('checked', false);
      $('.form-item-workflow-state').hide('slow');
    }

  });
};




})(jQuery);

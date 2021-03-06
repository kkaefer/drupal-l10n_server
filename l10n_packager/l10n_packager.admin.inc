<?php
// $Id: l10n_packager.admin.inc,v 1.1.2.1 2009-11-11 18:21:01 goba Exp $

/**
 * @file
 *   Administration pages for translation packaging.
 */

/**
 * Settings form callback.
 */
function l10n_packager_settings_form() {
  $form['l10n_packager_cron'] = array(
    '#title' => t('Generate packages on every Drupal cron run'),
    '#type' => 'checkbox',
    '#default_value' => variable_get('l10n_packager_cron', 0),
  );
  $form['l10n_packager_directory'] = array(
    '#title' => t('Directory for generated packages'),
    '#description' => t('The directory on the local file system to use to store packages generated. Either relative to the Drupal installation directory or an absolute path on your file system. Drupal should have read and write access to the files and directories found there.'),
    '#type' => 'textfield',
    '#required' => TRUE,
    '#default_value' => l10n_packager_directory(),
    // Create directory by default if possible.
    '#after_build' => array('l10n_packager_admin_check_directory'),
  );
  $form['l10n_packager_filepath'] = array(
    '#title' => t('Path structure for generated packages'),
    '#type' => 'textfield',
    '#required' => TRUE,
    '#default_value' => variable_get('l10n_packager_filepath', L10N_PACKAGER_FILEPATH),
    '#description' =>  t("Available tokens are: %project, %release, %core, %version, %extra, %language."),
  );
  $form['l10n_packager_release_limit'] = array(
    '#title' => t('Number of releases to check at once'),
    '#description' => t('The number of releases to check on a manual or cron run.'),
    '#type' => 'select',
    '#options' => drupal_map_assoc(array(1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 15, 20, 30, 40, 50)),
    '#default_value' => variable_get('l10n_packager_release_limit', 10),
  );
  $form['l10n_packager_file_limit'] = array(
    '#title' => t('Maximum number of files to package at once'),
    '#description' => t('The number of files to package on a manual or cron run.'),
    '#type' => 'select',
    '#options' => drupal_map_assoc(array(1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 15, 20, 30, 40, 50)),
    '#default_value' => variable_get('l10n_packager_file_limit', 1),
  );
  // Logging settings
  $period = array(0 => t('Never')) + drupal_map_assoc(array(3600, 10800, 21600, 32400, 43200, 86400, 172800, 259200, 604800, 1209600, 2419200, 4838400, 9676800), 'format_interval');
  $form['l10n_packager_update'] = array(
    '#title' => t('Repackaging interval'),
    '#type' => 'select',
    '#options' => $period,
    '#default_value' => variable_get('l10n_packager_update', 0),
    '#description' => t('Time interval for the translations to be automatically repackaged.'),
  );

  return system_settings_form($form);
}

/**
 * Check/create directory if not there already.
 */
function l10n_packager_admin_check_directory($form_element) {
  file_check_directory($form_element['#value'], FILE_CREATE_DIRECTORY, $form_element['#parents'][0]);
  return $form_element;
}

/**
 * Join two independent forms on one page.
 */
function l10n_packager_admin_repackage_page() {
  $output = drupal_get_form('l10n_packager_admin_projects_form');
  $output .= drupal_get_form('l10n_packager_admin_languages_form');
  return $output;
}

/**
 * Form callback. Repackage translations manually.
 */
function l10n_packager_admin_languages_form() {
  $form['languages'] = array(
    '#type' => 'fieldset',
    '#title' => t('List of languages'),
    '#description' => t('This list is rebuilt on every change to any languages, but you can force a rebuild here.'),
  );
  $form['languages']['submit'] = array(
    '#type' => 'submit',
    '#value' => t('Rebuild language list'),
    '#submit' => array('l10n_packager_export_language_list'),
  );
  return $form;
}

/**
 * Rebuild language list on form submission.
 */
function l10n_packager_admin_languages_form_submit() {
  l10n_packager_export_language_list();
}

/**
 * Form callback. Repackage translations manually.
 */
function l10n_packager_admin_projects_form() {
  $form['projects'] = array(
    '#type' => 'fieldset',
    '#title' => t('Project translations'),
    '#description' => t('Repackage all translations of a single project or release.'),
  );
  $form['projects']['project'] = array(
    '#title' => t('Project'),
    '#type' => 'textfield',
    '#autocomplete_path' => 'translate/projects/autocomplete',
    '#description' => t('Type the name of a project, eg. Drupal.'),
    '#required' => TRUE,
  );
  $form['projects']['release'] = array(
    '#title' => t('Release'),
    '#type' => 'textfield',
    '#description' => t('Optionally select a release name like <em>6.x-1.0-beta1</em> or a partial release name like <em>6.x%</em>.'),
  );
  $form['projects']['languages'] = array(
    '#type' => 'select',
    '#title' => t('Only for these languages'),
    '#multiple' => TRUE,
    '#default_value' => array(),
    '#options' => l10n_community_get_languages('name'),
    '#description' => t('Select none for all languages. Otherwise pick the languages you want repackaged.'),
  );

  $form['projects']['buttons']['repackage'] = array('#type' => 'submit', '#value' => t('Repackage now'));
  $form['projects']['buttons']['mark'] = array('#type' => 'submit', '#value' => t('Mark for repackaging'));
  return $form;
}

/**
 * Repackage form validation
 */
function l10n_packager_admin_projects_form_validate($form, &$form_state) {
  if (!empty($form_state['values']['project'])) {
    if ($project = l10n_community_get_projects(array('uri' => l10n_community_project_uri_by_title($form_state['values']['project'])))) {
      $form_state['values']['pid'] = $project->pid;
    }
    else {
      form_set_error('project', t('Invalid project name.'));
    }
  }
  if (!empty($form_state['values']['release'])) {
    $query = "SELECT COUNT(rid) FROM {l10n_community_release} WHERE title LIKE '%s'";
    $args = array($form_state['values']['release']);
    if ($project) {
      $query .= " AND pid = %d";
      $args[] = $project->pid;
    }
    if (!db_result(db_query($query, $args))) {
      form_set_error('release', t('Invalid release name.'));
    }
  }
  if (empty($form_state['values']['project']) && empty($form_state['values']['release'])) {
    form_set_error('project', t('You must select a project and optionally a release.'));
  }
}

/**
 * Repackage form submission
 */
function l10n_packager_admin_projects_form_submit($form, $form_state) {
  module_load_include('inc', 'l10n_packager');

  $op = $form_state['values']['op'];
  $languages = array_filter($form_state['values']['languages']);
  $pid = !empty($form_state['values']['pid']) ? $form_state['values']['pid'] : NULL;
  $release = !empty($form_state['values']['release']) ? $form_state['values']['release'] : NULL;

  // Prepare search parameters
  $where = $args = array();
  if ($pid) {
    $where[] = "pid = %d";
    $args[] = $pid;
  }
  if ($release) {
    $where[] = "title = '%s'";
    $args[] = $release;
  }
  // Build the query that will be used in different ways depending on the operation
  $query = 'SELECT rid FROM {l10n_community_release} WHERE '. implode(' AND ', $where);

  // Now check results and run operations
  if ($op == t('Repackage now')) {
    $result = db_query($query, $args);
    while ($release = db_fetch_object($result)) {
      $rids[] = $release->rid;
    }
    if (!empty($rids)) {
      $batch = l10n_packager_release_batch($rids, $languages);
      batch_set($batch);
    }
    else {
      drupal_set_message(t('No releases found for repackaging.'), 'error');
    }
  }
  elseif ($op == t('Mark for repackaging')) {
    $args = array_merge(array(L10N_PACKAGER_ACTIVE), $args);
    db_query("UPDATE {l10n_packager_release} SET updated = 0, checked = 0, status = %d WHERE rid IN ($query)", $args);
    drupal_set_message(t("Marked %count releases for repackaging.", array('%count' => db_affected_rows())));
  }
}

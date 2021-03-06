<?php
/*
 * Note, the table generated by this template that lists the publication
 * details is generic. It can be customized to look different for different
 * publication types.  To create a custom template for a given type, create a 
 * new file in the pub_types directory.  Name the file using the name of the
 * type. The name must be all lower-case and spaces should be replaced with
 * and underscore symbol.  For example, to create a custom table for the
 * "Conference Proceedings", create the file conference_proceedings.inc inside
 * of the pub_types folder.  Cut and paste the code below that generates the table
 * structure into the new file.  Then edit to your liking.
 * 
 */

$pub = $variables['node']->pub;

// expand the title
$pub = tripal_core_expand_chado_vars($pub, 'field', 'pub.title');
$pub = tripal_core_expand_chado_vars($pub, 'field', 'pub.volumetitle');

// get the citation
$values = array(
  'pub_id' => $pub->pub_id, 
  'type_id' => array(
    'name' => 'Citation',
  ),
);
$citation = tripal_core_generate_chado_var('pubprop', $values); 
$citation = tripal_core_expand_chado_vars($citation, 'field', 'pubprop.value');

// get the abstract
$values = array(
  'pub_id' => $pub->pub_id, 
  'type_id' => array(
    'name' => 'Abstract',
  ),
);
$abstract = tripal_core_generate_chado_var('pubprop', $values); 
$abstract = tripal_core_expand_chado_vars($abstract, 'field', 'pubprop.value');
$abstract_text = '';
if ($abstract) {
  $abstract_text = htmlspecialchars($abstract->value);
}

// get the author list
$values = array(
  'pub_id' => $pub->pub_id, 
  'type_id' => array(
    'name' => 'Authors',
  ),
);
$authors = tripal_core_generate_chado_var('pubprop', $values); 
$authors = tripal_core_expand_chado_vars($authors, 'field', 'pubprop.value');
$authors_list = 'N/A';
if ($authors) {
  $authors_list = $authors->value;
} 

// get the first database cross-reference with a url
$options = array('return_array' => 1);
$pub = tripal_core_expand_chado_vars($pub, 'table', 'pub_dbxref', $options);
$dbxref = NULL;
if ($pub->pub_dbxref) { 
  foreach ($pub->pub_dbxref as $index => $pub_dbxref) {
    if ($pub_dbxref->dbxref_id->db_id->urlprefix) {
      $dbxref = $pub_dbxref->dbxref_id;
    }
  }
}

// get the URL
// get the author list
$values = array(
  'pub_id' => $pub->pub_id, 
  'type_id' => array(
    'name' => 'URL',
  ),
);
$options = array('return_array' => 1);
$urls = tripal_core_generate_chado_var('pubprop', $values, $options); 
$urls = tripal_core_expand_chado_vars($urls, 'field', 'pubprop.value');
$url = '';
if (count($urls) > 0) {
  $url = $urls[0]->value; 
}?>

<div class="tripal_pub-data-block-desc tripal-data-block-desc"></div> <?php 

// to simplify the template, we have a subdirectory named 'pub_types'.  This directory
// should have include files each specific to a publication type. If the type is 
// not present then the base template will be used, otherwise the template in the
// include file is used.
$inc_name = strtolower(preg_replace('/ /', '_', $pub->type_id->name)) . '.inc';
$inc_path = drupal_realpath(drupal_get_path('module', 'tripal_pub') . "/theme/tripal_pub/pub_types/$inc_name");
if (file_exists($inc_path)) {
  require_once "pub_types/$inc_name";  
} 
else { 
  // ========================================================================
  // TO CUSTOMIZE A SPECIFIC PUBLICATION TYPE, CUT-AND-PASTE THE CODE
  // BELOW INTO A NEW FILE WITH THE SAME NAME AS THE TYPE (SEE INSTRUCTIONS
  // ABOVE), AND EDIT.
  // ========================================================================
  
  // the $headers array is an array of fields to use as the colum headers. 
  // additional documentation can be found here 
  // https://api.drupal.org/api/drupal/includes%21theme.inc/function/theme_table/7
  // This table for the analysis has a vertical header (down the first column)
  // so we do not provide headers here, but specify them in the $rows array below.
  $headers = array();
  
  // the $rows array contains an array of rows where each row is an array
  // of values for each column of the table in that row.  Additional documentation
  // can be found here:
  // https://api.drupal.org/api/drupal/includes%21theme.inc/function/theme_table/7 
  $rows = array();

  // Title row
  $title = '';
  if ($url) {
    $title =  l(htmlspecialchars($pub->title), $url, array('attributes' => array('target' => '_blank')));
  }
  elseif ($dbxref and $dbxref->db_id->urlprefix) {
    $title =  l(htmlspecialchars($pub->title), $dbxref->db_id->urlprefix . $dbxref->accession, array('attributes' => array('target' => '_blank')));
  }
  else {
    $title =  htmlspecialchars($pub->title);
  }
  $rows[] = array(
    array(
      'data' => 'Title',
      'header' => TRUE,
      'width' => '20%',
    ),
    $title,
  );
  // Authors row
  $rows[] = array(
    array(
      'data' => 'Authors',
      'header' => TRUE
    ),
    $authors_list,
  );
  // Type row
  $rows[] = array(
    array(
      'data' => 'Type',
      'header' => TRUE
    ),
    $pub->type_id->name,
  );
  // Media Title
  $rows[] = array(
    array(
      'data' => 'Media Title',
      'header' => TRUE,
      'nowrap' => 'nowrap'
    ),
    $pub->series_name,
  );
  // Volume
  $rows[] = array(
    array(
      'data' => 'Volume',
      'header' => TRUE
    ),
    $pub->volume ? $pub->volume : 'N/A',
  );
  // Issue
  $rows[] = array(
    array(
      'data' => 'Issue',
      'header' => TRUE
    ),
    $pub->issue ? $pub->issue : 'N/A'
  );
  // Year
  $rows[] = array(
    array(
      'data' => 'Year',
      'header' => TRUE
    ),
    $pub->pyear
  );
  // Pages
  $rows[] = array(
    array(
      'data' => 'Page(s)',
      'header' => TRUE
    ),
    $pub->pages ? $pub->pages : 'N/A'
  );
  // Citation row
  $rows[] = array(
    array(
      'data' => 'Citation',
      'header' => TRUE
    ),
    htmlspecialchars($citation->value)
  );
  // allow site admins to see the pub ID
  if (user_access('administer tripal')) {
    // Pub ID
    $rows[] = array(
      array(
        'data' => 'Pub ID',
        'header' => TRUE,
        'class' => 'tripal-site-admin-only-table-row',
      ),
      array(
        'data' => $pub->pub_id,
        'class' => 'tripal-site-admin-only-table-row',
      ),
    );
  }
  // Is Obsolete Row
  if($pub->is_obsolete == TRUE){
    $rows[] = array(
      array(
        'data' => '<div class="tripal_pub-obsolete">This publication is obsolete</div>',
        'colspan' => 2
      ),
    );
  }
  // the $table array contains the headers and rows array as well as other
  // options for controlling the display of the table.  Additional
  // documentation can be found here:
  // https://api.drupal.org/api/drupal/includes%21theme.inc/function/theme_table/7
  $table = array(
    'header' => $headers,
    'rows' => $rows,
    'attributes' => array(
      'id' => 'tripal_pub-table-base',
    ),
    'sticky' => FALSE,
    'caption' => '',
    'colgroups' => array(),
    'empty' => '',
  );
  
  // once we have our table array structure defined, we call Drupal's theme_table()
  // function to generate the table.
  print theme_table($table);
  if ($abstract_text) { ?>
    <p><b>Abstract</b></p>
    <div style="text-align: justify"><?php print $abstract_text; ?></div> <?php 
  } 
  
  // ========================================================================
  // END OF CUT-AND-PASTE REGION
  // ========================================================================
} 
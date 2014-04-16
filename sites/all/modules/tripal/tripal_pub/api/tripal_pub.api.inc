<?php
/**
 * @file
 * Provides an application programming interface (API) to manage chado publications
 */

/**
 * @defgroup tripal_pub_api Publication Module API
 * @ingroup tripal_api
 * @{
 * Provides an application programming interface (API) to manage chado publications
 *
 * @stephen add documentation here for how to add a new importer.
 *
 * @}
 */

/**
 * Retrieves a list of publications as an associated array where
 *  keys correspond directly with Tripal Pub CV terms.
 *
 * @param remote_db
 *    The name of the remote publication database to query. These names should
 *    match the name of the databases in the Chado 'db' table. Currently
 *    supported databass include
 *      'PMID':  PubMed
 *
 * @param search_array
 *    An associate array containing the search criteria. The following key
 *    are expected
 *      'remote_db':     Specifies the name of the remote publication database
 *      'num_criteria':  Specifies the number of criteria present in the search array
 *      'days':          The number of days to include in the search starting from today
 *      'criteria':      An associate array containing the search critiera. There should
 *                       be no less than 'num_criteria' elements in this array.
 *
 *    The following keys are expected in the 'criteria' array
 *      'search_terms':  A list of terms to search on, separated by spaces.
 *      'scope':         The fields to search in the remote database. Valid values
 *                       include: 'title', 'abstract', 'author' and 'any'
 *      'operation':     The logical operation to use for this criteria. Valid
 *                       values include: 'AND', 'OR' and 'NOT'.
 * @param $num_to_retrieve
 *    The number of records to retrieve.  In cases with large numbers of
 *    records to retrieve, the remote database may limit the size of each
 *    retrieval.
 * @param $page
 *    Optional.  If this function is called where the
 *    page for the pager cannot be set using the $_GET variable, use this
 *    argument to specify the page to retrieve.
 *
 * @return
 *   Returns an array of pubs where each element is
 *   an associative array where the keys are Tripal Pub CV terms.
 *
 * @ingroup tripal_pub_api
 */
function tripal_get_remote_pubs($remote_db, $search_array, $num_to_retrieve, $page = 0) {

  // now call the callback function to get the results
  $callback = "tripal_pub_remote_search_$remote_db";
  $pubs =  array(
    'total_records' => 0,
    'search_str'    => '',
    'pubs'          => array(),
  );
  if (function_exists($callback)) {
    $pubs = call_user_func($callback, $search_array, $num_to_retrieve, $page);
  }
  return $pubs;
}

/**
 * This function is used to perfom a query using one of the supported databases
 * and return the raw query results. This may be XML or some other format
 * as provided by the database.
 *
 * @param $dbxref
 *   The unique database ID for the record to retrieve.  This value must
 *   be of the format DB_NAME:ACCESSION where DB_NAME is the name of the
 *   database (e.g. PMID or AGL) and the ACCESSION is the unique identifier
 *   for the record in the database.
 *
 * @return
 *   Returns the publication array or FALSE if a problem occurs
 *
 * @ingroup tripal_pub_api
 */
function tripal_get_remote_pub($dbxref) {

  if(preg_match('/^(.*?):(.*?)$/', $dbxref, $matches)) {
    $remote_db = $matches[1];
    $accession = $matches[2];

    // check that the database is supported
    $supported_dbs = variable_get('tripal_pub_supported_dbs', array());
    if(!in_array($remote_db, $supported_dbs)) {
      return FALSE;
    }

    $search = array(
      'num_criteria' => 1,
      'remote_db' => $remote_db,
      'criteria' => array(
        '1' => array(
          'search_terms' => "$remote_db:$accession",
          'scope' => 'id',
          'operation' => '',
          'is_phrase' => 0,
        ),
      ),
    );
    $pubs = tripal_get_remote_pubs($remote_db, $search, 1, 0);

    return $pubs['pubs'][0];
  }
  return FALSE;
}

/**
 * Builds the SQL statement need to search Chado for the publications
 * that match the user supplied criteria.  Tpyically, this function is
 * called by the search form generated by the tripal_pub_search_form() function
 * but this function is included in the API for calling by anyone.
 *
 * @param $search_array
 *   An array of search criteria provided by the user. The search array is
 *   an associative array with the following keys:
 *     'num_criteria': an integer indicating the number of search criteria supplied
 *     'from_year':    filters records by a start year
 *     'to_year':      filters records by an end year
 *     'criteria':     an array of criteria. Each criteria is an associative
 *                     array with the following keys:
 *                     'search_terms':   The text used for searching
 *                     'scope':          The cvterm_id of the property used for filtering
 *                     'mode':           The operation (e.g. AND, OR or NOT)
 * @param $offset
 *   The offset for paging records.  The first record returned will be
 *   at the offset indicated here, and the next $limit number of records
 *   will be returned.
 *
 * @param $limit
 *   The number of records to retrieve
 *
 * @param total_records
 *   A value passed by reference. This value will get set to the total
 *   number of matching records
 *
 * @return
 *   a PDO database object of the query results.
 *
 * @ingroup tripal_pub
 */
function tripal_search_publications($search_array, $offset, $limit, &$total_records) {

  // build the SQL based on the criteria provided by the user
  $select = "SELECT DISTINCT P.*, CP.nid ";
  $from   = "FROM {pub} P
               LEFT JOIN public.chado_pub CP on P.pub_id = CP.pub_id
               INNER JOIN {cvterm} CVT on CVT.cvterm_id = P.type_id
            ";
  $where  = "WHERE (NOT P.title = 'null') "; // always exclude the dummy pub
  $order  = "ORDER BY P.pyear DESC, P.title ASC";
  $args = array();  // arguments for where clause
  $join = 0;

  $num_criteria = $search_array['num_criteria'];
  $from_year    = $search_array['from_year'];
  $to_year      = $search_array['to_year'];

  for ($i = 1; $i <= $num_criteria; $i++) {
    $value = $search_array['criteria'][$i]['search_terms'];
    $type_id = $search_array['criteria'][$i]['scope'];
    $mode = $search_array['criteria'][$i]['mode'];
    $op = $search_array['criteria'][$i]['operation'];

    // skip criteria with no values
    if(!$value) {
      continue;
    }

    // to prevent SQL injection make sure our operator is
    // what we expect
    if ($op and $op != "AND" and $op != "OR" and $op != 'NOT') {
      $op = 'AND';
    }
    if ($op == 'NOT') {
      $op = 'AND NOT';
    }
    if (!$op) {
      $op = 'AND';
    }

    // get the scope type
    $values = array('cvterm_id' => $type_id);
    $cvterm = chado_select_record('cvterm', array('name'), $values);
    $type_name = '';
    if (count($cvterm) > 0) {
      $type_name = $cvterm[0]->name;
    }
    if ($type_name == 'Title') {
      $where .= " $op (lower(P.title) LIKE lower(:crit$i)) ";
      $args[":crit$i"] = '%' . $value . '%';
    }
    elseif ($type_name == 'Year') {
      $where .= " $op (lower(P.pyear) = lower(:crit$i)) ";
      $args[":crit$i"] = '%' . $value . '%';
    }
    elseif ($type_name == 'Volume') {
      $where .= " $op (lower(P.volume) = lower(:crit$i)) ";
      $args[":crit$i"] = '%' . $value . '%';
    }
    elseif ($type_name == 'Issue') {
      $where .= " $op (lower(P.issue) = lower(:crit$i)) ";
      $args[":crit$i"] = '%' . $value . '%';
    }
    elseif ($type_name == 'Journal Name') {
      $from .= " LEFT JOIN {pubprop} PP$i ON PP$i.pub_id = P.pub_id AND PP$i.type_id = :crit$i ";
      $where .= " $op ((lower(P.series_name) = lower(:crit$i) and CVT.name = 'Journal Article') OR
      (lower(PP$i.value) = lower(:crit$i))) ";
      $args[":crit$i"] = $type_id;
    }
    elseif ($type_name == 'Conference Name') {
      $from .= " LEFT JOIN {pubprop} PP$i ON PP$i.pub_id = P.pub_id AND PP$i.type_id = :crit$i ";
      $where .= " $op ((lower(P.series_name) = lower(:crit$i) and CVT.name = 'Conference Proceedings') OR
      (lower(PP$i.value) = lower(:crit$i))) ";
      $args[":crit$i"] = $type_id;
    }
    elseif ($type_name == 'Publication Type') {
      $where .= " $op (lower(CVT.name) = lower(:crit$i))";
      $args[":crit$i"] = $value;
    }
    elseif ($type_id == 0) { //'Any Field'
      $from .= " LEFT JOIN {pubprop} PP$i ON PP$i.pub_id = P.pub_id ";
      $where .= " $op (lower(PP$i.value)  LIKE lower(:crit$i) OR
      lower(P.title) LIKE lower(:crit$i) OR
      lower(P.volumetitle) LIKE lower(:crit$i) OR
      lower(P.publisher) LIKE lower(:crit$i) OR
      lower(P.uniquename) LIKE lower(:crit$i) OR
      lower(P.pubplace) LIKE lower(:crit$i) OR
      lower(P.miniref) LIKE lower(:crit$i) OR
      lower(P.series_name) LIKE lower(:crit$i)) ";
      $args[":crit$i"] = '%' . $value . '%';
    }
    // for all other properties
    else {
      $from .= " LEFT JOIN {pubprop} PP$i ON PP$i.pub_id = P.pub_id AND PP$i.type_id = :type_id$i ";
      $where .= " $op (lower(PP$i.value) LIKE lower(:crit$i)) ";
      $args[":crit$i"] = '%' . $value . '%';
      $args[":type_id$i"] = $type_id;
    }
  }
  if($from_year and $to_year) {
    $where .= " AND (P.pyear ~ '....' AND to_number(P.pyear,'9999') >= :from$i AND to_number(P.pyear,'9999') <= :to$i) ";
    $args[":from$i"] = $from_year;
    $args[":to$i"] = $to_year;
  }
  $sql = "$select $from $where $order  LIMIT " . (int) $limit . ' OFFSET ' . (int) $offset;
  $count = "SELECT count(*) FROM ($select $from $where $order) as t1";

  // first get the total number of matches
  $total_records = chado_query($count, $args)->fetchField();
  $results = chado_query($sql, $args);

  return $results;
}

/**
 * Retrieves a chado publication array
 *
 * @param $identifier
 *   An array used to uniquely identify a publication. This array has the same
 *   format as that used by the chado_generate_var(). The following keys can be
 *   useful for uniquely identifying a publication as they should be unique:
 *    - pub_id: the chado pub.pub_id primary key
 *    - nid: the drupal nid of the publication
 *    - uniquename: A value to matach with the pub.uniquename field
 *   There are also some specially handled keys. They are:
 *    - property: An array describing the property to select records for. It
 *      should at least have either a 'type_name' key (if unique across cvs) or
 *      'type_id' key. Other supported keys include: 'cv_id', 'cv_name' (of the type),
 *      'value' and 'rank'
 *    - dbxref: The database cross reference accession.  It should be in the form
 *        DB:ACCESSION, where DB is the database name and ACCESSION is the
 *        unique publication identifier (e.g. PMID:4382934)
 *    - dbxref_id:  The dbxref.dbxref_id of the publication.
 * @param $options
 *   An array of options. Supported keys include:
 *     - Any keys supported by chado_generate_var(). See that function definition for
 *       additional details.
 *
 * NOTE: the $identifier parameter can really be any array similar to $values passed into
 *   chado_select_record(). It should fully specify the pub record to be returned.
 *
 * @return
 *   If a singe publication is retreived using the identifiers, then a publication
 *   array will be returned.  The array is of the same format returned by the
 *   chado_generate_var() function. Otherwise, FALSE will be returned.
 *
 * @ingroup tripal_pub_api
 */
function tripal_get_publication($identifiers, $options = array()) {

  // Error Checking of parameters
  if (!is_array($identifiers)) {
    tripal_report_error('tripal_pub_api', TRIPAL_ERROR,
      "chado_get_publication: The identifier passed in is expected to be an array with the key
       matching a column name in the pub table (ie: pub_id or name). You passed in %identifier.",
      array('%identifier'=> print_r($identifiers, TRUE))
    );
  }
  elseif (empty($identifiers)) {
    tripal_report_error('tripal_pub_api', TRIPAL_ERROR,
      "chado_get_publication: You did not pass in anything to identify the publication you want. The identifier
       is expected to be an array with the key matching a column name in the pub table
       (ie: pub_id or name). You passed in %identifier.",
      array('%identifier'=> print_r($identifiers, TRUE))
    );
  }

  // If one of the identifiers is property then use chado_get_record_with_property()
  if (array_key_exists('property', $identifiers)) {
    $property = $identifiers['property'];
    unset($identifiers['property']);
    $pub = chado_get_record_with_property('pub', $property, $identifiers, $options);
  }
  elseif (array_key_exists('dbxref', $identifiers)) {
    if(preg_match('/^(.*?):(.*?)$/', $identifiers['dbxref'], $matches)) {
      $dbname = $matches[1];
      $accession = $matches[2];

      $values = array(
        'dbxref_id' => array (
          'accession' => $accession,
          'db_id' => array(
            'name' => $dbname
          ),
        ),
      );
      $pub_dbxref = chado_select_record('pub_dbxref', array('pub_id'), $values);
      if (count($pub_dbxref) > 0) {
        $pub = chado_generate_var('pub', array('pub_id' => $pub_dbxref[0]->pub_id), $options);
      }
      else {
        return FALSE;
      }
    }
    else {
      tripal_report_error('tripal_pub_api', TRIPAL_ERROR,
        "chado_get_publication: The dbxref identifier is not correctly formatted.",
        array('%identifier'=> print_r($identifiers, TRUE))
      );
    }
  }
  elseif (array_key_exists('dbxref_id', $identifiers)) {
    // first get the pub_dbxref record
    $values = array('dbxref_id' => $identifiers['dbxref_id']);
    $pub_dbxref = chado_select_record('pub_dbxref', array('pub_id'), $values);

    // now get the pub
    if (count($pub_dbxref) > 0) {
      $pub = chado_generate_var('pub', array('pub_id' => $pub_dbxref[0]->pub_id), $options);
    }
    else {
      return FALSE;
    }

  }
  // Else we have a simple case and we can just use chado_generate_var to get the pub
  else {
    // Try to get the pub
    $pub = chado_generate_var('pub', $identifiers, $options);
  }

  // Ensure the pub is singular. If it's an array then it is not singular
  if (is_array($pub)) {
    tripal_report_error('tripal_pub_api', TRIPAL_ERROR,
      "chado_get_publication: The identifiers did not find a single unique record. Identifiers passed: %identifier.",
      array('%identifier'=> print_r($identifiers, TRUE))
    );
  }

  // Report an error if $pub is FALSE since then chado_generate_var has failed
  elseif ($pub === FALSE) {
    tripal_report_error('tripal_pub_api', TRIPAL_ERROR,
      "chado_get_publication: Could not find a publication using the identifiers
       provided. Check that the identifiers are correct. Identifiers passed: %identifier.",
      array('%identifier'=> print_r($identifiers, TRUE))
    );
  }

  // Else, as far we know, everything is fine so give them their pub :)
  else {
    return $pub;
  }
}
/**
 * The publication table of Chado only has a unique constraint for the
 * uniquename of the publiation, but in reality a publication can be considered
 * unique by a combination of the title, publication type, published year and
 * series name (e.g. journal name or conference name). The site administrator
 * can configure how publications are determined to be unique.  This function
 * uses the configuration specified by the administrator to look for publications
 * that match the details specified by the $pub_details argument
 * and indicates if one ore more publications match the criteria.
 *
 * @param $pub_details
 *   An associative array with details about the publications. The expected keys
 *   are:
 *     'Title':              The title of the publication
 *     'Year':               The published year of the publication
 *     'Publication Type':   An array of publication types. A publication can have more than one type.
 *     'Series Name':        The series name of the publication
 *     'Journal Name':       An alternative to 'Series Name'
 *     'Conference Name':    An alternative to 'Series Name'
 *     'Citation':           The publication citation (this is the value saved in the pub.uniquename field and must be unique)
 *   If this key is present it will also be checked
 *     'Publication Dbxref': A database cross reference of the form DB:ACCESSION where DB is the name
 *                           of the database and ACCESSION is the unique identifier (e.g PMID:3483139)
 *
 * @return
 *   An array containing the pub_id's of matching publications. Returns an
 *   empty array if no pubs match
 */
function tripal_publication_exists($pub_details) {


  // first try to find the publication using the accession number if that key exists in the details array
  if (array_key_exists('Publication Dbxref', $pub_details)) {
    $pub = chado_get_publication(array('dbxref' => $pub_details['Publication Dbxref']));
    if($pub) {
      return array($pub->pub_id);
    }
  }

  // make sure the citation is unique
  if (array_key_exists('Citation', $pub_details)) {
    $pub = chado_get_publication(array('uniquename' => $pub_details['Citation']));
    if($pub) {
      return array($pub->pub_id);
    }
  }

  // get the publication type (use the first publication type)
  if (array_key_exists('Publication Type', $pub_details)) {
    $type_name = '';
    if(is_array($pub_details['Publication Type'])) {
      $type_name = $pub_details['Publication Type'][0];
    }
    else {
      $type_name = $pub_details['Publication Type'];
    }
    $identifiers = array(
      'name' => $type_name,
      'cv_id' => array(
        'name' => 'tripal_pub',
      ),
    );
    $pub_type = tripal_get_cvterm($identifiers);
  }
  else {
    tripal_report_error('tripal_pub', TRIPAL_ERROR, 
      "chado_does_pub_exist(): The Publication Type is a " .
      "required property but is missing", array());
    return array();
  }
  if (!$pub_type) {
    tripal_report_error('tripal_pub', TRIPAL_ERROR, 
     "chado_does_pub_exist(): Cannot find publication type: '%type'",
      array('%type' => $pub_details['Publication Type'][0]));
    return array();
  }

  // get the series name.  The pub.series_name field is only 255 chars so we must truncate to be safe
  $series_name = '';
  if (array_key_exists('Series Name', $pub_details)) {
    $series_name = substr($pub_details['Series Name'], 0, 255);
  }
  if (array_key_exists('Journal Name', $pub_details)) {
    $series_name = substr($pub_details['Journal Name'], 0, 255);
  }
  if (array_key_exists('Conference Name', $pub_details)) {
    $series_name = substr($pub_details['Conference Name'], 0, 255);
  }
  
  // make sure the publication is unique using the prefereed import duplication check
  $import_dups_check = variable_get('tripal_pub_import_duplicate_check', 'title_year_media');
  $pubs = array();
  switch ($import_dups_check) {
    case 'title_year':
      $identifiers = array(
        'title' => $pub_details['Title'],
        'pyear' => $pub_details['Year']
      );
      $pubs = chado_select_record('pub', array('pub_id'), $identifiers);
      break;
    case 'title_year_type':
      $identifiers = array(
        'title'   => $pub_details['Title'],
        'pyear'   => $pub_details['Year'],
        'type_id' => $pub_type->cvterm_id,
      );
      $pubs = chado_select_record('pub', array('pub_id'), $identifiers);
      break;
    case 'title_year_media':
      $identifiers = array(
        'title'       => $pub_details['Title'],
        'pyear'       => $pub_details['Year'],
        'series_name' => $series_name,
      );
      $pubs = chado_select_record('pub', array('pub_id'), $identifiers);
      break;
  }

  $return = array();
  foreach ($pubs as $pub) {
    $return[] = $pub->pub_id;
  }

  return $return;
}

/**
 * Updates publication records that currently exist in the Chado pub table
 * with the most recent data in the remote database.
 *
 * @param $do_contact
 *   Set to TRUE if authors should automatically have a contact record added
 *   to Chado. Contacts are added using the name provided by the remote
 *   database.
 * @param $dbxref
 *   The unique database ID for the record to update.  This value must
 *   be of the format DB_NAME:ACCESSION where DB_NAME is the name of the
 *   database (e.g. PMID or AGL) and the ACCESSION is the unique identifier
 *   for the record in the database.
 * @param $db
 *   The name of the remote database to update.  If this value is provided and
 *   no dbxref then all of the publications currently in the Chado database
 *   for this remote database will be updated.
 *
 * @ingroup tripal_pub_api
 */
function tripal_reimport_publications($do_contact = FALSE, $dbxref = NULL, $db = NULL) {

  print "\nNOTE: Loading of publications is performed using a database transaction. \n" .
      "If the load fails or is terminated prematurely then the entire set of \n" .
      "insertions/updates is rolled back and will not be found in the database\n\n";
  $transaction = db_transaction();
  try {

    // get a list of all publications by their Dbxrefs that have supported databases
    $sql = "
      SELECT DB.name as db_name, DBX.accession
      FROM pub P
        INNER JOIN pub_dbxref PDBX ON P.pub_id = PDBX.pub_id
        INNER JOIN dbxref DBX      ON DBX.dbxref_id = PDBX.dbxref_id
        INNER JOIN db DB           ON DB.db_id = DBX.db_id
    ";
    $args = array();
    if ($dbxref and preg_match('/^(.*?):(.*?)$/', $dbxref, $matches)) {
      $dbname = $matches[1];
      $accession = $matches[2];
      $sql .= "WHERE DBX.accession = :accession and DB.name = :dbname ";
      $args[':accession'] = $accession;
      $args[':dbname'] = $dbname;
    }
    elseif ($db) {
      $sql .= " WHERE DB.name = :dbname ";
      $args[':dbname'] = $db;
    }
    $sql .= "ORDER BY DB.name, P.pub_id";
    $results = chado_query($sql, $args);

    $num_to_retrieve = 100;
    $i = 0;                 // count the number of IDs. When we hit $num_to_retrieve we'll do the query
    $curr_db = '';          // keeps track of the current current database
    $ids = array();         // the list of IDs for the database
    $search = array();      // the search array passed to the search function

    // iterate through the pub IDs
    while ($pub = $results->fetchObject()) {
      $accession = $pub->accession;
      $remote_db = $pub->db_name;

      // here we need to only update publications for databases we support
      $supported_dbs = variable_get('tripal_pub_supported_dbs', array());
      if(!in_array($remote_db, $supported_dbs)) {
        continue;
      }
      $search = array(
        'num_criteria' => 1,
        'remote_db' => $remote_db,
        'criteria' => array(
          '1' => array(
            'search_terms' => "$remote_db:$accession",
            'scope' => 'id',
            'operation' => '',
            'is_phrase' => 0,
          ),
        ),
      );
      $pubs = tripal_get_remote_pubs($remote_db, $search, 1, 0);
      tripal_pub_add_publications($pubs, $do_contact, TRUE);

      $i++;
    }

    // sync the newly added publications with Drupal
    print "Syncing publications with Drupal...\n";
    chado_node_sync_records('pub');

    // if the caller wants to create contacts then we should sync them
    if ($do_contact) {
      print "Syncing contacts with Drupal...\n";
      chado_node_sync_records('contact');
    }
  }
  catch (Exception $e) {
    print "\n"; // make sure we start errors on new line
    watchdog_exception('T_pub_import', $e);
    $transaction->rollback();
    print "FAILED: Rolling back database changes...\n";
    return;
  }
  print "Done.\n";
}

/**
 * Imports all publications for a given publication import setup.
 *
 * @param $import_id
 *   The ID of the import setup to use
 * @param $job_id
 *   The jobs management job_id for the job if this function is run as a job.
 *
 * @ingroup tripal_pub_api
 */
function tripal_execute_pub_importer($import_id, $job_id = NULL) {
  print "\nNOTE: Loading of publications is performed using a database transaction. \n" .
        "If the load fails or is terminated prematurely then the entire set of \n" .
        "insertions/updates is rolled back and will not be found in the database\n\n";

  // start the transaction
  $transaction = db_transaction();

  try {
    $page = 0;
    $do_contact = FALSE;
    $num_to_retrieve = 100;

    // get all of the loaders
    $args = array(':import_id' => $import_id);
    $sql = "SELECT * FROM {tripal_pub_import} WHERE pub_import_id = :import_id ";
    $import = db_query($sql, $args)->fetchObject();

    print "Executing Importer: '" . $import->name . "'\n";

    $criteria = unserialize($import->criteria);
    $remote_db = $criteria['remote_db'];
    $total_pubs = 0;
    do {
      // retrieve the pubs for this page. We'll retreive 100 at a time
      $results  = tripal_get_remote_pubs($remote_db, $criteria, $num_to_retrieve, $page);
      $pubs     = $results['pubs'];
      $num_pubs = $rseults['total_records'];
      $total_pubs += $num_pubs;
      tripal_pub_add_publications($pubs, $import->do_contact);
      $page++;
    }
    // continue looping until we have a $pubs array that does not have
    // our requested numer of records.  This means we've hit the end
    while (count($pubs) == $num_to_retrieve);

    // sync the newly added publications with Drupal. If the user
    // requested a report then we don't want to print any syncing information
    // so pass 'FALSE' to the sync call
    print "Syncing publications with Drupal...\n";
    chado_node_sync_records('pub');

    // if any of the importers wanted to create contacts from the authors then sync them
    if($import->do_contact) {
      print "Syncing contacts with Drupal...\n";
      chado_node_sync_records('contact');
    }
    tripal_set_job_progress($job_id, '100');
  }
  catch (Exception $e) {
    print "\n"; // make sure we start errors on new line
    watchdog_exception('T_pub_import', $e);
    $transaction->rollback();
    print "FAILED: Rolling back database changes...\n";
    return;
  }
  print "Done.\n";
}

/**
 * Imports all publications for all active import setups.
 *
 * @param $report_email
 *   A list of email address, separated by commas, that should be notified
 *   once importing has completed
 * @param $do_update
 *   If set to TRUE then publications that already exist in the Chado database
 *   will be updated, whereas if FALSE only new publications will be added
 *
 * @ingroup tripal_pub_api
 */
function tripal_execute_active_pub_importers($report_email = FALSE, $do_update = FALSE) {
  $num_to_retrieve = 100;
  $page = 0;

  print "\nNOTE: Loading of publications is performed using a database transaction. \n" .
      "If the load fails or is terminated prematurely then the entire set of \n" .
      "insertions/updates is rolled back and will not be found in the database\n\n";

  // start the transaction
  $transaction = db_transaction();

  try {
    // get all of the loaders
    $args = array();
    $sql = "SELECT * FROM {tripal_pub_import} WHERE disabled = 0 ";
    $results = db_query($sql, $args);
    $do_contact = FALSE;
    $reports = array();
    foreach ($results as $import) {
      $page = 0;
      print "Executing importer: '" . $import->name . "'\n";
      // keep track if any of the importers want to create contacts from authors
      if ($import->do_contact == 1) {
        $do_contact = TRUE;
      }
      $criteria = unserialize($import->criteria);
      $remote_db = $criteria['remote_db'];
      do {
        // retrieve the pubs for this page. We'll retreive 100 at a time
        $results = tripal_get_remote_pubs($remote_db, $criteria, $num_to_retrieve, $page);
        $pubs = $results['pubs'];
        $reports[$import->name] = tripal_pub_add_publications($pubs, $import->do_contact, $do_update);
        $page++;
      }
      // continue looping until we have a $pubs array that does not have
      // our requested numer of records.  This means we've hit the end
      while (count($pubs) == $num_to_retrieve);
    }

    // sync the newly added publications with Drupal. If the user
    // requested a report then we don't want to print any syncing information
    // so pass 'FALSE' to the sync call
    print "Syncing publications with Drupal...\n";
    chado_node_sync_records('pub');

    // iterate through each of the reports and generate a final report with HTML links
    $HTML_report = '';
    if ($report_email) {
      $HTML_report .= "<html>";
      global $base_url;
      foreach ($reports as $importer => $report) {
        $total = count($report['inserted']);
        $HTML_report .= "<b>$total new publications from importer: $importer</b><br><ol>\n";
        foreach ($report['inserted'] as $pub) {
          $item = $pub['Title'];
          if (array_key_exists('pub_id', $pub)) {
            $item = l($pub['Title'], "$base_url/pub/" . $pub['pub_id']);
          }
          $HTML_report .= "<li>$item</li>\n";
        }
        $HTML_report .= "</ol>\n";
      }
      $HTML_report .= "</html>";
      $site_email = variable_get('site_mail', '');
      $params = array(
        'message' => $HTML_report
      );
      drupal_mail('tripal_pub', 'import_report', $report_email, language_default(), $params, $site_email, TRUE);
    }

    // if any of the importers wanted to create contacts from the authors then sync them
    if($do_contact) {
      print "Syncing contacts with Drupal...\n";
      chado_node_sync_records('contact');
    }
  }
  catch (Exception $e) {
    print "\n"; // make sure we start errors on new line
    watchdog_exception('T_pub_import', $e);
      $transaction->rollback();
      print "FAILED: Rolling back database changes...\n";
    return;
  }
  print "Done.\n";
}

/**
 * Imports a singe publication specified by a remote database cross reference.
 *
 * @param $pub_dbxref
 *   The unique database ID for the record to update.  This value must
 *   be of the format DB_NAME:ACCESSION where DB_NAME is the name of the
 *   database (e.g. PMID or AGL) and the ACCESSION is the unique identifier
 *   for the record in the database.
 * @param $do_contact
 *   Set to TRUE if authors should automatically have a contact record added
 *   to Chado.
 * @param $do_update
 *   If set to TRUE then the publication will be updated if it already exists
 *   in the database.
 *
 * @ingroup tripal_pub_api
 */
function tripal_import_pub_by_dbxref($pub_dbxref, $do_contact = FALSE, $do_update) {
  $num_to_retrieve = 1;
  $pager_id = 0;
  $page = 0;
  $num_pubs = 0;

  print "\nNOTE: Loading of publications is performed using a database transaction. \n" .
      "If the load fails or is terminated prematurely then the entire set of \n" .
      "insertions/updates is rolled back and will not be found in the database\n\n";

  $transaction = db_transaction();
  try {
    if(preg_match('/^(.*?):(.*?)$/', $pub_dbxref, $matches)) {
      $dbname = $matches[1];
      $accession = $matches[2];

      $criteria = array(
        'num_criteria' => 1,
        'remote_db' => $dbname,
        'criteria' => array(
          '1' => array(
            'search_terms' => "$dbname:$accession",
            'scope' => 'id',
            'operation' => '',
            'is_phrase' => 0,
          ),
        ),
      );
      $remote_db = $criteria['remote_db'];
      $results = tripal_get_remote_pubs($remote_db, $criteria, $num_to_retrieve, $page);
      $pubs          = $results['pubs'];
      $search_str    = $results['search_str'];
      $total_records = $results['total_records'];
      $pub_id = tripal_pub_add_publications($pubs, $do_contact, $do_update);
    }

    // sync the newly added publications with Drupal
    print "Syncing publications with Drupal...\n";
    chado_node_sync_records('pub');

    // if any of the importers wanted to create contacts from the authors then sync them
    if($do_contact) {
      print "Syncing contacts with Drupal...\n";
      chado_node_sync_records('contact');
    }
  }
  catch (Exception $e) {
    print "\n"; // make sure we start errors on new line
    watchdog_exception('T_pub_import', $e);
        $transaction->rollback();
        print "FAILED: Rolling back database changes...\n";
      return;
  }

  print "Done.\n";
}
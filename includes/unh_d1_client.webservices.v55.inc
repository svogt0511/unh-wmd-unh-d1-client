<?php

/**
 * @file
 *
 * @todo: Update this docblock.
 * @todo: move program area setting to this module
 *
 * Implements DestinyOne v5.4/5.5 API requests, specifically using REST/JSON.
 */

/******************************************************************************
 *
 * API service call functions (and additional functionality) - v5.4
 *
 ******************************************************************************/

/**
 * Implements a webservice request.
 *
 */
function unh_d1_client_request($request = '', $baseEntity = '', $options = array(), $criteria = array(), $method = 'GET') {
  $ret = array(
    '500',
    'Unable to make webservice request - $request = ' . $request . (!empty($baseEntity) ? '$baseEntity = ' . $baseEntity : ''),
    NULL,
  );

  if (array_key_exists($request, UNH_D1_CLIENT_API_REQUESTS)) {
  
    $url_opts = _unh_d1_client_build_request_opts($request, $options);

    // Set url. Don't allow '/' character to be part of the course code (which is used as the base entity in the url path).
    $url = variable_get('unh_d1_client_webservice_path') . (!empty($request) ? '/' . $request : '') . (!empty($baseEntity) ? '/' . trim($baseEntity) : '');
    $url .= (!empty($url_opts) ? '?' . drupal_http_build_query($url_opts) : '');
    
    // Set header(s)/body opts.
    $opts['method'] = $method;
    
    if (strtoupper($method) == 'POST') {
      $opts['headers']['Content-Type'] = UNH_D1_CLIENT_WEBSVC_RQST_FMT;
      $opts['headers']['accept'] = UNH_D1_CLIENT_WEBSVC_RQST_FMT;
      $body_opts = _unh_d1_client_build_search_criteria($request, $criteria, $options);
      $opts['data'] = json_encode($body_opts);
    }

    // Determine cache status.
    $d1_cache = &drupal_static('d1_requests');
    $key = _unh_d1_client_cache_key($url . http_build_query($opts));

    if (isset($d1_cache[$key])) {
      $ret = $d1_cache[$key];
      //ddl('GETTING CACHED VALUE!!');
    }
    else {
      // Make the request!
      $result = drupal_http_request($url, $opts);
      // Check for service unavailable and forward to appropriate page. Otherwise, just
      // return $result
      // ddl($result, '$result - MAKING REQUEST');
      // ddl($url, '$url - MAKING REQUEST');
      // ddl($opts, '$opts - MAKING REQUEST');

      $code = (int)$result->code;
      if ((($code >= 500) && ($code <=599)) || 
        ($code == HTTP_REQUEST_TIMEOUT))
        // ($code == HTTP_REQUEST_TIMEOUT) || 
        // (property_exists($result, 'error') && 
        // isset($result->error) && 
        // ($result->error == UNH_D1_CLIENT_SERVICE_UNAVAILABLE))
      {
        drupal_goto('/service-unavailable');
      }
      
      if ($result->code == '200') {
        $decoded_result = json_decode($result->data, TRUE);
        $ret = array($result->code, NULL, $decoded_result);
        $d1_cache[$key] = $ret;
        // ddl("STORING CACHED VALUE!!");
      }
      else {
        $msg = UNH_D1_CLIENT_GENERIC_ERROR;
        if (property_exists($result, 'error') && isset($result->error)) {
          $msg = $result->error;
        }
        // $ret = array($result->code, 'error is: ' . $result->data, NULL);
        $ret = array($result->code, $msg, NULL);
      }
    }
  }

  return $ret;
}

/******************************************************************************
 *
 * API option-building functions - v5.4
 *
 * Creates/adds search options to the options structure which can then be
 * converted to json for the actual call.
 *
 * Build the search criteria structure for a v54 request based on the request
 * structure as defined in the D1CLIENT_API_REQUESTS definition.

 * If an unknown request is submitted, it is ignored.
 * If an unknown arg/param exists, it is ignored.
 *
 * $requests - matches the api request string
 * $args - an key/value pair where value may be:
 *    a 'scalar' value
 *    an array of key/'scalar values'
 *    a data structure (ex: paginationConstruct)
 *    an array of key/data structures (ex: userDefinedFields)
 *
 * key must match the key of parameter of the specific API request.
 * value must fit the requirements of its associated key.  If value is a
 *    data structure or an array of data structures, the structure must match
 *    the corresponding structure in the API request.
 *
 * The idea is to simplify the building of the request structures, not to
 * have to recode the request structure for every request.
 * (Was not done well in 5.3)
 *
 * However, we are counting on parameters that are structures to be built and
 * passed in correctly, for ex. 'programAreas' is an array['programAreaCode'].
 * (which makes no sense, but it doesn't work otherwise - REPORT BUG.)
 *
 ******************************************************************************/

/**
 * Build the opts that go in the url request (GET/POST)
 *
 * Given a request name, returns the basic request structure and with defaults.
 */
function _unh_d1_client_build_request_opts($request, $args) {
  $ret = array();
  
  if (array_key_exists($request, UNH_D1_CLIENT_API_REQUESTS)) {
    $base = UNH_D1_CLIENT_API_REQUESTS[$request];

    $ret = $base['opts']['defaults'];
    
    foreach($args as $key=>$arg) {
      if (in_array($key, $base['opts']['params'])) {
        $ret[$key] = $arg;
      }    
    }
  }
  return $ret;
}

/**
 * Builds the request body for POST requests.
 *
 * Given an array of key=>value, builds a json-encoded argument
 * structure for a webservice request.
 *
 * The reason behind this is to allow easy building of post requests and to
 * localize knowledge of that in this one function. (So if it changes we only
 * have to change it in one place.)
 *
 * @todo: finish this function!
 */
function _unh_d1_client_build_search_criteria($request, $args, $options) {
  $ret = array();
  
  // Test to see if we have a valid request.
  if (array_key_exists($request, UNH_D1_CLIENT_API_REQUESTS)) {
    // Get the definition of the request.
    $base = UNH_D1_CLIENT_API_REQUESTS[$request];
    
    // Build a default request structure.
    $opts = _unh_d1_client_build_default_request_body($request);

    if (!empty($args)) {
      // Add in the args for the request.
      $opts = _unh_d1_client_build_request_body($request, $opts, $args, $options);
    }
    
    $ret = $opts;  
  }

  return $ret;
}

/**
 * Builds a request body. (POST)
 *
 * Given a request name, default request body ($opts) and a set of args,
 * returns the basic request structure and with defaults.
 */  
function _unh_d1_client_build_request_body($request, $opts, $args, $options) {
  $ret = $opts;

  if (array_key_exists($request, UNH_D1_CLIENT_API_REQUESTS)) {
    $base = UNH_D1_CLIENT_API_REQUESTS[$request];

    $detail_key = $base['requestDetail']['label'];
    $criteria_key = $base['requestCriteria']['label'];
    $advanced_key = $base['advancedCriteria']['label'];

    foreach($args as $key=>$arg) {
      if (in_array($key, $base['base']['params'])) {
        if (array_key_exists($key, $base['base']['builder'])) {
          $arg = $base['base']['builder'][$key]($arg);
        }
        if (!empty($arg)) {
          $ret[$key] = $arg;
        }
      } elseif (in_array($key, $base['requestDetail']['params'])) {
        if (array_key_exists($key, $base['requestDetail']['builder'])) {
          $arg = $base['requestDetail']['builder'][$key]($arg);
        }
        if (!empty($arg)) {
          $ret[$detail_key][$key] = $arg;
        }
      } elseif (in_array($key, $base['requestCriteria']['params'])) {
        if (array_key_exists($key, $base['requestCriteria']['builder'])) {
          $arg = $base['requestCriteria']['builder'][$key]($arg);
        }
        if (!empty($arg)) {
          $ret[$detail_key][$criteria_key][$key] = $arg;
        }
      } elseif (in_array($key, $base['advancedCriteria']['params'])) {
        if (array_key_exists($key, $base['advancedCriteria']['builder'])) {
        $function = $base['advancedCriteria']['builder'][$key];
          $arg = $base['advancedCriteria']['builder'][$key]($arg);
        }
        if (!empty($arg)) {
          $ret[$detail_key][$criteria_key][$advanced_key][$key] = $arg;
        }
      } // else { best effort - invalid options are being ignored }
    }
  }
  
  // Special check - delete overrideMaxResults if other than short is specified
  // for informationLevel
  if (array_key_exists('informationLevel', $options) && (strcasecmp('short', $options['informationLevel']) !== 0)) {
    if (array_key_exists('overrideMaxResults', $ret[$detail_key])) {
      unset($ret[$detail_key]['overrideMaxResults']);
    }
  }
    
  return $ret;
}

/**
 * Builds the default body of a request (POST)
 *
 * Given a request name, returns the basic request structure and with defaults.
 */
function _unh_d1_client_build_default_request_body($request) {
  $ret = array();
  
  if (array_key_exists($request, UNH_D1_CLIENT_API_REQUESTS)) {
    $base = UNH_D1_CLIENT_API_REQUESTS[$request];

    // Build a default request body structure.
    $opts = [
      $base['requestDetail']['label'] => [
        $base['requestCriteria']['label'] => [
          $base['advancedCriteria']['label'] =>[]
        ],
      ],
    ];    

    $detail_key = $base['requestDetail']['label'];
    $criteria_key = $base['requestCriteria']['label'];
    $advanced_key = $base['advancedCriteria']['label'];

    $opts = array_merge(
      $opts,
      $base['base']['defaults']  
    );

    $opts[$detail_key] = array_merge(
      $opts[$detail_key],
      $base['requestDetail']['defaults']   
    );

    $opts[$detail_key][$criteria_key] = array_merge(
      $opts[$detail_key][$criteria_key],
      $base['requestCriteria']['defaults']   
    );

    $opts[$detail_key][$criteria_key][$advanced_key] = array_merge(
      $opts[$detail_key][$criteria_key][$advanced_key],
      $base['advancedCriteria']['defaults']   
    );

    $ret = $opts;
  }
  
  return $ret;
}

/**
 * Implements key calculation for the drupal static cache.
 *
 * Given a fully-qualified url, (http://... or https://... 
 * concatenated with any 'stringified' arguments, returns a key for
 * the Drupal static hash array.
 *
 * The purpose of this is to allow us to cache requests in Drupal static
 * so we don't have to make the same request twice per page load.
 */
function _unh_d1_client_cache_key($str) {
  $ret = '';

  $ret = hash('crc32', $str, FALSE);

  return $ret;
}


/**
 * Builds the programAreas part of the POST body. (BUG - D1)
 *
 * Returns keyed array with the last programAreaCode specified. Uses the last
 * programAreaCode given.
 *
 * Should be able to just use an indexed array, but if a list of programAreaCodes
 * is presented as an indexed array, D1 ignores them.
 *
 * This is kind of funky because of a bug in the API (json).
 * We only use the last programArea because there is no way we can express
 * multiple programAreas in their json interpretation of the request.
 *
 * @todo: submit bug report - programAreas
 */
function _unh_d1_client_build_programAreas($programAreas) {
  $ret = array();
  
  if (!empty($programAreas)) {
    if (is_array($programAreas)) {
      foreach($programAreas as $programArea) {
        $ret['programAreaCode'] = $programArea;
      }
    } else {
      $ret['programAreaCode'] = $programAreas;
    }
  }

  return $ret;
}


function unh_d1_client_get_pagination_response($decoded_result) {
  $res = NULL;
  
  if (array_key_exists('SearchCourseSectionProfileResult', $decoded_result) && array_key_exists('paginationResponse', $decoded_result['SearchCourseSectionProfileResult'])) {
    $res = $decoded_result['SearchCourseSectionProfileResult']['paginationResponse'];
  }
  
  return $res;
}

function unh_d1_client_got_full_response($pagination_response) {
  $res = TRUE;
  
  if (!empty($pagination_response)) {
    if ($pagination_response['totalCount'] > ($pagination_response['pageSize'] * $pagination_response['pageNumber'])) {
      $res = FALSE;
    }
  }

  return $res;
}











 

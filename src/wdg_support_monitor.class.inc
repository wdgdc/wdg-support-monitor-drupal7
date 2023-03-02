<?php
/**
 * @file WDG Support Monitor Class
 * @author Web Development Group (WDG) <https://www.webdevelopmentgroup.com>
 */

final class WDG_Support_Monitor {

  /**
   * Name of our last run setting
   *
   * @access public
   */
  const LAST_RUN_KEY = 'wdg_support_monitor_last_run';

  /**
   * API Endpoint
   *
   * @var string
   */
  private $api_endpoint;

  /**
   * API Secret
   *
   * @var string
   */
  private $api_secret;

  /**
   * Site URL
   *
   * @var string
   */
  private $url = '';

  /**
   * Last run data
   *
   * @var array|null
   */
  private $last_run;

  /**
   * Project data
   *
   * @var array|null
   */
  private $project_data;

  /**
   * Singleton instance
   *
   * @access private
   */
  private static $_instance;

  /**
   * Singleton method
   *
   * @access public
   */
  public static function get_instance() {
    if ( ! isset( self::$_instance ) ) {
      self::$_instance = new self();
    }

    return self::$_instance;
  }

  /**
   * Constructor
   */
  private function __construct() {
    if ( defined( 'WDG_SUPPORT_MONITOR_API_ENDPOINT' ) && ! empty( WDG_SUPPORT_MONITOR_API_ENDPOINT ) ) {
      $this->api_endpoint = WDG_SUPPORT_MONITOR_API_ENDPOINT;
    }

    if ( defined( 'WDG_SUPPORT_MONITOR_API_SECRET' ) && ! empty( WDG_SUPPORT_MONITOR_API_SECRET ) ) {
      $this->api_secret = WDG_SUPPORT_MONITOR_API_SECRET;
    }

    $this->url = $GLOBALS['base_url'];
  }

  /**
   * Is the module configured correctly?
   *
   * @return bool
   */
  public function is_configured() {
    if ( empty( $this->api_endpoint ) || empty( $this->api_secret ) ) {
      return false;
    }

    if ( filter_var( $this->api_endpoint, FILTER_VALIDATE_URL ) === false ) {
      // Invalid API Endpoint
      return false;
    }

    return true;
  }

  /**
   * Get module info
   *
   * @access public
   * @return object
   */
  public function info() {

    $url = $this->url;

    if ( defined( 'WDG_SUPPORT_MONITOR_SITE_URL' ) && ! empty( WDG_SUPPORT_MONITOR_SITE_URL ) ) {
      $url = WDG_SUPPORT_MONITOR_SITE_URL;
    }

    $data = new \StdClass;
    $data->api_endpoint = $this->api_endpoint;
    $data->api_secret = $this->api_secret;
    $data->url = $url;

    return $data;
  }

  /**
   * Get last run data
   *
   * @access public
   * @return array|null
   */
  public function get_last_run() {
    if ( ! empty( $this->last_run ) ) {
      return $this->last_run;
    }

    // Fetch variable
    $this->last_run = variable_get( self::LAST_RUN_KEY, NULL );

    return $this->last_run;
  }

  /**
   * Set last run data
   *
   * @access private
   * @param object $data
   */
  private function set_last_run( $data ) {
    $this->last_run = $data;

    // Update data
    variable_set( self::LAST_RUN_KEY, $this->last_run );
  }

  /**
   * Get Project Data
   * Note: this is very intensive and should not be done on the front-end
   *
   * @see update_get_projects internal functions to generate project list with enabled and disabled modules
   *
   * @access private
   * @return array|false
   */
  private function get_project_data() {
    if ( ! empty( $this->project_data ) ) {
      return $this->project_data;
    }

    // @todo https://github.com/drush-ops/drush/blob/7.x/commands/pm/updatestatus.pm.inc
    // @todo https://github.com/drush-ops/drush/blob/7.x/lib/Drush/UpdateService/StatusInfoDrupal7.php

    // Trigger update fetch
    $available = update_get_available( TRUE );
    if ( empty( $available ) ) {
      return FALSE;
    }

    // Load module file containing internal functions
    module_load_include('inc', 'update', 'update.compare');

    // @see update_get_projects()
    $projects = array();
    $module_data = system_rebuild_module_data();
    $theme_data = system_rebuild_theme_data();
    _update_process_info_list( $projects, $module_data, 'module', TRUE );
    _update_process_info_list( $projects, $module_data, 'module', FALSE );
    _update_process_info_list( $projects, $theme_data, 'theme', TRUE );
    _update_process_info_list( $projects, $theme_data, 'theme', FALSE );

    // @see update_calculate_project_data()
    update_process_project_info( $projects );
    foreach( $projects as $project => $project_info ) {
      if ( isset( $available[ $project ] ) ) {
        // Sets ['recommended'] key
        update_calculate_project_update_status( $project, $projects[ $project ], $available[ $project ] );
      } else {
        // Update unknown
      }
    }

    // Store project data
    $this->project_data = $projects;
    return $this->project_data;
  }

  /**
   * Compile core data
   *
   * @access private
   * @return object
   */
  private function compile_core() {
    $data = new \StdClass();
    $data->current = VERSION;

    $project_data = $this->get_project_data();
    $data->recommended = ! empty( $project_data['drupal']['recommended'] ) ? $project_data['drupal']['recommended'] : NULL;

    return $data;
  }

  /**
   * Compile addon data
   *
   * @access private
   * @return array
   */
  private function compile_addons() {
    $data = array();

    $project_data = $this->get_project_data();
    if ( empty( $project_data ) ) {
      return $data;
    }

    foreach( $project_data as $project_name => $project ) {
      if ( 'drupal' === $project_name ) continue; // Ignore core

      $addon = new \StdClass();
      $addon->name = $project_name;
      $addon->display = $project['info']['name'];
      $addon->type = preg_replace( '/-disabled$/', '', $project['project_type'] ); // Strip -disabled suffix
      $addon->current = $project['existing_version'];
      $addon->recommended = ! empty( $project['recommended'] ) ? $project['recommended'] : NULL;
      $addon->active = $project['project_status'] === true;

      array_push( $data, $addon );
    }

    return $data;
  }

  /**
   * Compile report data
   *
   * @access private
   * @param int $timestamp Compile timestamp
   * @return object
   */
  private function compile( $timestamp ) {
    $url = $this->url;

    if ( defined( 'WDG_SUPPORT_MONITOR_SITE_URL' ) && ! empty( WDG_SUPPORT_MONITOR_SITE_URL ) ) {
      $url = WDG_SUPPORT_MONITOR_SITE_URL;
    }

    // Key is hash of site URL, secret, and timestamp
    $key = hash( 'sha256', $url . $this->api_secret . $timestamp );

    // Compile data
    $data = new \StdClass;
    $data->url = $url;
    $data->timestamp = $timestamp;
    $data->key = $key;
    $data->core = $this->compile_core();
    $data->addons = $this->compile_addons();

    return $data;
  }

  /**
   * Compile report
   *
   * @access public
   * @return object
   */
  public function report() {
    return $this->compile( REQUEST_TIME );
  }

  /**
   * Push update to WDG support
   *
   * @access public
   * @param bool $blocking
   * @return object|string Last run object on success, string error message on failure
   */
  public function update( $blocking = false ) {
    if ( ! $this->is_configured() ) {
      return sprintf( 'Unable to update! Incorrect configuration!' );
    }

    flush(); // flush the output just in case we're on the front end

    $data = $this->compile( REQUEST_TIME );

    $options = array(
      'method' => 'POST',
      'data' => json_encode( $data ),
      'headers' => array(
        'Content-Type' => 'application/json'
      ),
      'timeout' => 5, // Short timeout (default 30)
    );

    $response = drupal_http_request( $this->api_endpoint, $options );

    // Store last run regardless of success
    $last_run = new \StdClass();
    $last_run->success = $response->code >= 200 && $response->code < 300;
    $last_run->timestamp = REQUEST_TIME;
    $last_run->request = new \StdClass();
    $last_run->request->url = $this->api_endpoint;
    $last_run->request->options = $options;
    $last_run->response = $response;
    $this->set_last_run( $last_run );

    if ( ! $last_run->success ) {
      // Failed!
      return sprintf( 'Unable to update! Response code: %s - Message: %s', $response->code, $response->status_message );
    }

    return $last_run;
  }

}

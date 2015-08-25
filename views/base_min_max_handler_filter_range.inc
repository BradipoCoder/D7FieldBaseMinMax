<?php

/**
 * @file
 * Definition of views_handler_filter_numeric.
 */

/**
 * Simple filter to handle greater than/less than filters
 *
 * @ingroup views_filter_handlers
 */
class base_min_max_handler_filter_range extends views_handler_filter {
  /** @var views_plugin_query_default $query */
  var $query = NULL;

  /** @var bool $debug */
  private $debug = false;

  /** @var array */
  protected static $handlerChecks;

  public function init(&$view, &$options) {
    $this->always_multiple = FALSE;
    parent::init($view, $options);
  }

  public function option_definition() {
    $options = parent::option_definition();

    $options['operator'] = ['default' => 'between'];
    $options['value'] = [
      'contains' => [
        'min_range' => ['default' => 0],
        'max_range' => ['default' => 100],
        'min_value' => ['default' => 0],
        'max_value' => ['default' => 100],
        'slider_min_max_override' => ['default' => 1],
        'slider_step' => ['default' => 1],
        'filter_margin' => ['default' => 0],
        'filter_use_column_1' => ['default' => 'base'],
        'filter_use_column_2' => ['default' => 'base'],
      ],
    ];

    $options['expose']['contains']['identifier']['default'] = $this->definition['field_name'];
    $options['expose']['contains']['label']['default'] = $this->definition['title short'];
    $options['exposed'] = array('default' => TRUE, 'bool' => TRUE);
    return $options;
  }

  /**
   * Provide a list of all the numeric operators
   */
  function operator_options() {
    return [
      'between' => 'Range',
      '=' => 'Equal to',
      '>=' => 'Greater than(>=)',
      '<=' => 'Less than(<=)',
    ];
  }


  /**
   * Exposed Filter Form
   * @param array $form
   * @param array $form_state
   */
  protected function value_form_exposed(&$form, &$form_state) {
    $identifier = $this->options['expose']['identifier'];

    $this->prepareData($form_state);
    $this->overrideFilters($form_state);

    $form['value'] = [];

    // If min_range is the same as max_range we have no need for this filter
    if($this->value['min_range'] == $this->value['max_range']) {
      $this->view->filter[$identifier]->options["exposed"] = false;
      unset($this->view->display_handler->handlers['filter'][$identifier]);
      return;
    }

    $this->attachBootstrapSlider($form);

    $SV = [
      'minVal' => $this->value['min_value'],
      'maxVal' => $this->value['max_value'],
    ];

    if ($this->operator == "between") {
      $dataSliderValue = '[' . implode(',', $SV) . ']';
    }
    else {
      $dataSliderValue = $SV['minVal'];
    }

    $form['value']['#tree'] = TRUE;

    $form['value']['slider'] = [
      '#type' => 'textfield',
      '#title' => '',
      '#size' => 10,
      '#attributes' => [
        'class' => ['bootstrap-slider', 'ctools-auto-submit-exclude'],
        'data-identifier' => $identifier,
        'data-slider-min' => $this->value['min_range'],
        'data-slider-max' => $this->value['max_range'],
        'data-slider-value' => $dataSliderValue,
        'data-slider-step' => $this->options['value']['slider_step'],
      ],
    ];

    $form['value']['op'] = [
      '#type' => 'hidden',
      '#title' => 'Operation',
      '#size' => 10,
      '#value' => "",
      '#attributes' => [
        'style' => ['width:10%;']
      ]
    ];

    if ($this->debug) {
      $form['value']['mrk'] = [
        '#markup' => '<pre style="max-width:17%;font-size:10px;">'
                     . htmlentities(drupal_render($form['value']['slider']))
                     . '</pre>',
      ];
      $form['value']['slider']['#printed'] = FALSE;
    }

  }


  /**
   * @todo: implement caching for this data
   * @param \views_handler_filter|\stdClass $filter
   * @return array|boolean
   */
  protected function getHandlerChecksDataForFilter($filter) {
    $answer = FALSE;
    $filterSignature = $this->getSignature($filter);
    if (isset(self::$handlerChecks["data"][$filterSignature])) {
      $answer = self::$handlerChecks["data"][$filterSignature];
    }
    return $answer;
  }

  /**
   * @param \views_handler_filter|\stdClass $filter
   * @return array|boolean
   */
  protected function getViewCloneResults($filter) {
    if (!$filter || isset($this->view->is_copy)) {
      return FALSE;
    }

    if (!$this->getHandlerChecksDataForFilter($filter)) {
      /**
       * We use $filterSignature to keep query results cached
       */
      $filterSignature = $this->getSignature($filter);
      //
      $viewClone = $this->view->clone_view();
      $viewClone->is_copy = TRUE;
      $viewClone->copy_id = $filterSignature;
      $viewClone->set_arguments($this->view->args);
      $viewClone->set_items_per_page(0);

      //Remove current filter from exposed inputs and all other filters which are not changed/used(sliders only)
      $exposedInput = [];
      foreach ($filter->view->exposed_input as $ik => $input) {
        if ($ik != $filter->options['expose']['identifier']) {
          $addInput = true;
          if(isset($input["slider"])) {
            $fieldChanged = (isset($input["op"])&&$input["op"]=="change");
            $fieldUnset = (isset($input["op"])&&$input["op"]=="unset");
            $fieldUsed = self::sessionVar($ik, "in_use");
            $addInput = $fieldChanged || ($fieldUsed && !$fieldUnset);
          }
          if($addInput) {
            $exposedInput[$ik] = $input;
          }
        }
      }
      $viewClone->set_exposed_input($exposedInput);

      $viewClone->execute($this->view->current_display);

      if ($viewClone->executed) {
        $minMaxResult = $this->getMinMaxFromViewResults($viewClone);
        self::$handlerChecks["data"][$filterSignature] = $minMaxResult;
      }

      $viewClone->destroy();
    }

    return $this->getHandlerChecksDataForFilter($filter);
  }

  /**
   * @param $form_state
   */
  protected function prepareData(&$form_state) {
    if (isset($this->view->is_copy)) {
      return;
    }
    if (isset(self::$handlerChecks["data"])) {
      return;
    }
    self::$handlerChecks["data"] = [];

    //Unfiltered results
    $filter = $this->getFakeFilterForUnfilteredResults();
    $this->getViewCloneResults($filter);
    if ($this->debug) {
      dpm($this->getHandlerChecksDataForFilter($filter), "UNFILTERED DATA");
    }

    //Filtered results
    if (isset($this->view->filter) && count($this->view->filter)) {
      foreach ($this->view->filter as $fKey => $filter) {
        if ($filter instanceof $this) {
          $this->getViewCloneResults($filter);
          if ($this->debug) {
            dsm($filter->options["expose"]["identifier"] . " === " . $this->getSignature($filter));
            dpm($this->getHandlerChecksDataForFilter($filter), "DATA(".$this->getSignature($filter).")");
          }
        }
      }
    }
  }


  /**
   * Overrides min_range/max_range values for exposed filters from db data where:
   * - 'slider_min_max_override' is set to true (configuration)
   * - filter is not already applied (not filtered)
   * @param array $form_state
   */
  protected function overrideFilters(&$form_state) {
    if (isset($this->view->is_copy)) {
      return;
    }
    $identifier = $this->options['expose']['identifier'];

    // Columns to use
    $lowColumn = $this->value['filter_use_column_1'];
    $highColumn = $this->value['filter_use_column_2'];

    // Result Sets
    $unfilteredResults = $this->getHandlerChecksDataForFilter($this->getFakeFilterForUnfilteredResults());
    $filteredResults = $this->getHandlerChecksDataForFilter($this);

    // Static Min/Max
    $min_range_static = $this->value["min_range"];
    $max_range_static = $this->value["max_range"];

    // Unfiltered Min/Max
    $min_range_unfiltered = isset($unfilteredResults[$identifier][$lowColumn]["min"]) ? $unfilteredResults[$identifier][$lowColumn]["min"] : $min_range_static;
    $max_range_unfiltered = isset($unfilteredResults[$identifier][$highColumn]["max"]) ? $unfilteredResults[$identifier][$highColumn]["max"] : $max_range_static;

    // Filtered Min/Max
    $min_range_filtered = isset($filteredResults[$identifier][$lowColumn]["min"]) ? $filteredResults[$identifier][$lowColumn]["min"] : $min_range_unfiltered;
    $max_range_filtered = isset($filteredResults[$identifier][$highColumn]["max"]) ? $filteredResults[$identifier][$highColumn]["max"] : $max_range_unfiltered;

    //Field Operation
    $fieldOperation = isset($form_state['input'][$identifier]['op']) ? $form_state['input'][$identifier]['op'] : "";

    //Is field in use?
    if ($fieldOperation == "unset") {
      $fieldInUse = false;
    } else if ($fieldOperation == "change") {
      $fieldInUse = true;
    } else {
      $fieldInUse = self::sessionVar($identifier, "in_use");
    }
    $fieldInUse = self::sessionVar($identifier, "in_use", $fieldInUse);

    // Get Current Slider Value(s)
    $CSV = $this->getSliderValuesFrom(
      isset($form_state['input'][$identifier]['slider']) ? $form_state['input'][$identifier]['slider'] : FALSE,
      $min_range_unfiltered,
      $max_range_unfiltered
    );

    if($this->value["slider_min_max_override"]) {
      // Calculate min and max for range
      $min_range = $min_range_filtered;
      $max_range = $max_range_filtered;
      if ($fieldInUse) {
        $min_range = self::sessionVar($identifier, "min_range");
        $max_range = self::sessionVar($identifier, "max_range");
        //check
        $min_range = $min_range ? $min_range : $min_range_unfiltered;
        $max_range = $max_range ? $max_range : $max_range_unfiltered;
      }
    } else {
      $min_range = $min_range_static;
      $max_range = $max_range_static;
    }

    // Calculate min and max for values
    $min_value = $CSV["minVal"];
    $max_value = $CSV["maxVal"];
    // If this filter was not applied maximize to values to the entire range
    if(!$fieldInUse) {
      $min_value = $min_range;
      $max_value = $max_range;
    }

    //Set Values
    $this->value['min_value'] = $min_value;
    $this->value['max_value'] = $max_value;
    $this->value['min_range'] = $min_range;
    $this->value['max_range'] = $max_range;

    //Store in session
    self::sessionVar($identifier, "min_value", $min_value);
    self::sessionVar($identifier, "max_value", $max_value);
    self::sessionVar($identifier, "min_range", $min_range);
    self::sessionVar($identifier, "max_range", $max_range);

    //Show what's going on
    if ($this->debug) {
      dsm(
        "[$identifier] Min/Max Override:"
        . "\n  OVERRIDE: " . ($this->value["slider_min_max_override"] ? "YES" : "NO")
        . "\n  FIELD-OP: " . $fieldOperation
        . "\nFLD-IN-USE: " . ($fieldInUse ? "YES" : "NO")
        . "\n    STATIC: " . json_encode(['minVal' => $min_range_static, 'maxVal' => $max_range_static])
        . "\nUNFILTERED: " . json_encode(['minVal' => $min_range_unfiltered, 'maxVal' => $max_range_unfiltered])
        . "\n  FILTERED: " . json_encode(['minVal' => $min_range_filtered, 'maxVal' => $max_range_filtered])
      );
    }
  }


  /**
   * Decide weather or not modify query
   * @param array $input
   * @return bool
   */
  public function accept_exposed_input($input) {
    $answer = FALSE;
    $input = $this->view->exposed_input;
    $identifier = $this->options['expose']['identifier'];

    if (isset($this->view->is_copy)) {
      if($this->debug) {
        dsm("[" . $this->view->copy_id . "]($identifier)" . ": " . json_encode($input));
      }
      if (isset($input[$identifier]) && isset($input[$identifier]["slider"]) && !empty($input[$identifier]["slider"])) {

        // Get Current Slider Value(s)
        $CSV = $this->getSliderValuesFrom(
          $input[$identifier]["slider"],
          $this->value['min_value'],
          $this->value['max_value']
        );

        $this->value["min_value"] = $CSV['minVal'];
        $this->value["max_value"] = $CSV['maxVal'];

        $answer = TRUE;
      }
    }
    else {
      $answer = TRUE;
    }
    return $answer;
  }

  /**
   * Modifies query
   */
  public function query() {
    $this->ensure_my_table();
    try {
      $filter_margin = floatval($this->options['value']['filter_margin']);
      $minValue = floatval($this->value['min_value']) - $filter_margin;
      $maxValue = floatval($this->value['max_value']) + $filter_margin;
      $dbColumnLow = $this->table_alias . '.'
                     . $this->getDatabaseColumnName($this->options['value']['filter_use_column_1']);
      $dbColumnHigh = $this->table_alias . '.'
                      . $this->getDatabaseColumnName($this->options['value']['filter_use_column_2']);

      switch ($this->operator) {
        case "between":
          $this->query->add_where($this->options['group'], $dbColumnLow, [$minValue, $maxValue], 'BETWEEN');
          if ($dbColumnLow != $dbColumnHigh) {
            $this->query->add_where($this->options['group'], $dbColumnHigh, [$minValue, $maxValue], 'BETWEEN');
          }
          break;
        case "=":
          $this->query->add_where($this->options['group'], $dbColumnLow, [$minValue, $maxValue], 'BETWEEN');
          break;
        case ">=":
          $this->query->add_where($this->options['group'], $dbColumnLow, $minValue, '>=');
          break;
        case "<=":
          $this->query->add_where($this->options['group'], $dbColumnLow, $maxValue, '<=');
          break;
      }
    } catch(Exception $e) {
      drupal_set_message($e->getMessage(), 'error');
    }
  }


  /**
   * Elaborate result in view and get min/max result for each filter and for each key
   * @param \view $view
   * @return array
   */
  protected function getMinMaxFromViewResults($view) {
    $keys = ["base", "minimum", "maximum"];
    $answer = [];

    $handlers = [];
    if (isset($view->filter) && count($view->filter)) {
      foreach ($view->filter as $fKey => $filter) {
        if ($filter instanceof $this) {
          $fieldName = $filter->definition["field_name"];
          if (isset($view->field[$fieldName])) {
            $handlers[] = $view->field[$fieldName];
            $answer[$fieldName] = array_fill_keys($keys, ["min" => 0, "max" => 0]);
          }
        }
      }
    }

    if (count($handlers)) {
      if (isset($view->result) && is_array($view->result) && count($view->result)) {
        foreach ($view->result as $ri => $row) {
          /** @var \views_handler_field_field $handler */
          foreach ($handlers as $handler) {
            $fieldName = $handler->definition["field_name"];
            $fieldData = $handler->get_value($row);
            if (is_array($fieldData) && count($fieldData) && isset($fieldData[0])) {
              $fieldData = $fieldData[0];
              foreach ($keys as $key) {
                if (isset($fieldData[$key])) {
                  // Find lowest MIN
                  if (!$answer[$fieldName][$key]["min"] || $fieldData[$key] < $answer[$fieldName][$key]["min"]) {
                    $answer[$fieldName][$key]["min"] = floatval($fieldData[$key]);
                  }
                  // Find highest MAX
                  if (!$answer[$fieldName][$key]["max"] || $fieldData[$key] > $answer[$fieldName][$key]["max"]) {
                    $answer[$fieldName][$key]["max"] = floatval($fieldData[$key]);
                  }
                }
              }
            }
          }
        }
      }
    }

    return $answer;
  }

  /**
   * Modify expose form field configuration
   * @param array $form
   * @param array $form_state
   */
  function expose_form(&$form, &$form_state) {
    parent::expose_form($form, $form_state);
    //
    $form['operator']['#prefix'] = '';
    $form['operator']['#suffix'] = '';
    $form['value']['#prefix'] = '';
    $form['value']['#suffix'] = '';
    //
    $form['expose']['use_operator'] = ['#type' => 'value', '#value' => 0];
    $form['expose']['operator_label'] = ['#type' => 'value', '#value' => ''];
    $form['expose']['multiple'] = ['#type' => 'value', '#value' => 0];

  }

  /**
   * Filter Settings / Exposed Filter Forms
   * @param array $form
   * @param array $form_state
   */
  public function value_form(&$form, &$form_state) {
    if (!empty($form_state['exposed'])) {
      $this->value_form_exposed($form, $form_state);
    }
    else {
      $this->value_form_settings($form, $form_state);
    }
  }

  /**
   * Filter Settings Form
   * @param array $form
   * @param array $form_state
   */
  protected function value_form_settings(&$form, &$form_state) {
    $mrki = 1;

    $form['value'] = [
      '#tree' => TRUE,
      '#type' => 'fieldset',
      '#title' => t('Slider settings'),
    ];

    $form['value']['mrk' . $mrki++] = ['#markup' => '<table>'];

    // Values
    $form['value']['mrk' . $mrki++] = ['#markup' => '<tr>'];
    $form['value']['min_range'] = [
      '#type' => 'textfield',
      '#title' => t('Range Minimum'),
      '#size' => 10,
      '#default_value' => $this->value['min_range'],
      '#required' => TRUE,
      '#prefix' => '<td>',
      '#suffix' => '</td>',
    ];

    $form['value']['max_range'] = [
      '#type' => 'textfield',
      '#title' => t('Range Maximum'),
      '#size' => 10,
      '#default_value' => $this->value['max_range'],
      '#required' => TRUE,
      '#prefix' => '<td>',
      '#suffix' => '</td>',
    ];

    $form['value']['min_value'] = [
      '#type' => 'textfield',
      '#title' => t('Default Minimum'),
      '#size' => 10,
      '#default_value' => $this->value['min_value'],
      '#required' => TRUE,
      '#prefix' => '<td>',
      '#suffix' => '</td>',
    ];

    $form['value']['max_value'] = [
      '#type' => 'textfield',
      '#title' => t('Default Maximum'),
      '#size' => 10,
      '#default_value' => $this->value['max_value'],
      '#required' => TRUE,
      '#prefix' => '<td>',
      '#suffix' => '</td>',
    ];
    $form['value']['mrk' . $mrki++] = ['#markup' => '</tr>'];

    //Filter Columns
    //dsm("SRC: " . $source);
    $form['value']['mrk' . $mrki++] = ['#markup' => '<tr>'];
    $form['value']['filter_use_column_1'] = [
      '#type' => 'select',
      '#title' => t('Column to filter(lower)'),
      '#description' => 'The column to mach in the database. If "Range" operator is used this will be the used for the Minimum value.',
      '#options' => ['base' => 'Base', 'minimum' => 'Minimum', 'maximum' => 'Maximum'],
      '#default_value' => $this->value['filter_use_column_1'],
      '#required' => TRUE,
      '#prefix' => '<td colspan="2">',
      '#suffix' => '</td>',
    ];
    $form['value']['filter_use_column_2'] = [
      '#type' => 'select',
      '#title' => t('Column to filter(higher)'),
      '#description' => 'The column to mach in the database. If "Range" operator is used this will be the used for the Maximum value. For all other operators this will be ignored.',
      '#options' => ['base' => 'Base', 'minimum' => 'Minimum', 'maximum' => 'Maximum'],
      '#default_value' => $this->value['filter_use_column_2'],
      '#required' => TRUE,
      '#prefix' => '<td colspan="2">',
      '#suffix' => '</td>',
    ];
    $form['value']['mrk' . $mrki++] = ['#markup' => '</tr>'];

    $form['value']['mrk' . $mrki++] = ['#markup' => '<tr>'];
    $form['value']['slider_min_max_override'] = [
      '#type' => 'checkbox',
      '#title' => t('Minimum/Maximum Override'),
      '#description' => t('If enabled, the minimum and maximum values for the range will be narrowed down to the available values.'),
      '#default_value' => $this->value['slider_min_max_override'],
      '#prefix' => '<td colspan="4">',
      '#suffix' => '</td>',
    ];
    $form['value']['mrk' . $mrki++] = ['#markup' => '</tr>'];

    $form['value']['mrk' . $mrki++] = ['#markup' => '<tr>'];
    $form['value']['slider_step'] = [
      '#type' => 'textfield',
      '#title' => t('Slider Step'),
      '#description' => t('The amount by which the slider can be moved.'),
      '#size' => 10,
      '#default_value' => $this->value['slider_step'],
      '#required' => TRUE,
      '#prefix' => '<td colspan="4">',
      '#suffix' => '</td>',
    ];
    $form['value']['mrk' . $mrki++] = ['#markup' => '</tr>'];

    $form['value']['mrk' . $mrki++] = ['#markup' => '<tr>'];
    $form['value']['filter_margin'] = [
      '#type' => 'textfield',
      '#title' => t('Filter Margin'),
      '#description' => t('Allow this much of difference between slider value and the actual value.'),
      '#size' => 10,
      '#default_value' => $this->value['filter_margin'],
      '#required' => TRUE,
      '#prefix' => '<td colspan="4">',
      '#suffix' => '</td>',
    ];
    $form['value']['mrk' . $mrki++] = ['#markup' => '</tr>'];


    $form['value']['mrk' . $mrki++] = ['#markup' => '</table>'];
  }

  /**
   * @return string
   */
  public function admin_summary() {
    $output = '';
    if ($this->is_exposed()) {
      $output .= t('exposed');
    }
    return $output;
  }

  /**
   * @return bool
   */
  public function can_build_group() {
    return FALSE;
  }

  /**
   * @param string  $sliderValue - either a single numeric value OR a comma separated list of two figures
   * @param integer $defMin
   * @param integer $defMax
   * @return array
   */
  protected function getSliderValuesFrom($sliderValue, $defMin = 0, $defMax = 0) {
    $minVal = $defMin;
    $maxVal = $defMax;
    //
    if ($sliderValue) {
      if (strpos($sliderValue, ",")) {
        list($minVal, $maxVal) = explode(",", $sliderValue);
      }
      else {
        $minVal = $maxVal = $sliderValue;
      }
    }
    $answer = [
      "minVal" => floatval($minVal),
      "maxVal" => floatval($maxVal)
    ];
    return $answer;
  }

  /**
   * @return \stdClass
   */
  protected function getFakeFilterForUnfilteredResults() {
    $filter = new \stdClass();
    $filter->view = new \stdClass();
    $filter->options['expose']['identifier'] = 'fake_filter_for_unfiltered_results';
    $filter->view->name = $this->view->name;
    $filter->view->args = $this->view->args;
    $filter->view->exposed_input = ["unfiltered" => TRUE];
    $filter->definition['field_name'] = '';
    $filter->field = '';
    $filter->table = '';
    return $filter;
  }

  /**
   * Get a signature for filter handler.
   *
   * @param \views_handler_filter|\stdClass $filter
   * @return string
   */
  protected function getSignature($filter) {
    $data = [
      'identifier' => $filter->options['expose']['identifier'],
      'view_name' => $filter->view->name,
      'view_args' => $filter->view->args,
      'view_input' => $filter->view->exposed_input,
      'table' => $filter->table,
      'field_name' => $filter->definition['field_name'],
      'field' => $filter->field,
    ];
    return md5(serialize($data));
  }

  /**
   * @param string $filterIdentifier
   * @param string $key
   * @param null   $val
   * @return bool
   */
  protected static function sessionVar($filterIdentifier, $key, $val = NULL) {
    if (drupal_session_started() === FALSE) {
      drupal_session_start();
    }
    $answer = FALSE;
    $sessionIdentifier = 'minMaxBaseViewHandlerFilterRange';
    if ($val !== NULL) {
      $_SESSION[$sessionIdentifier][$filterIdentifier][$key] = serialize($val);
    }
    if (isset($_SESSION[$sessionIdentifier][$filterIdentifier][$key])) {
      $answer = unserialize($_SESSION[$sessionIdentifier][$filterIdentifier][$key]);
    }
    return $answer;
  }

  /**
   * @param string $type
   * @return string
   * @throws \Exception
   */
  protected function getDatabaseColumnName($type) {
    $types = ['base', 'minimum', 'maximum'];
    if (in_array($type, $types)) {
      return ($this->definition["field_name"] . '_' . $type);
    }
    else {
      throw new \Exception(
        __CLASS__ . " - Unknown database column type: " . $type . "! Allowed: " . json_encode($types)
      );
    }
  }

  /**
   * attach bootstrap-slider to form
   * @param array $form
   */
  protected function attachBootstrapSlider(&$form) {
    if (!isset($form["#attached"])) {
      $form["#attached"] = [];
    }
    if (!isset($form["#attached"]["css"])) {
      $form["#attached"]["css"] = [];
    }
    if (!isset($form["#attached"]["js"])) {
      $form["#attached"]["js"] = [];
    }
    $form["#attached"]["css"][] = drupal_get_path('module', 'base_min_max')
                                  . '/libraries/bootstrap-slider/bootstrap-slider.css';
    $form["#attached"]["js"][] = drupal_get_path('module', 'base_min_max')
                                 . '/libraries/bootstrap-slider/bootstrap-slider.min.js';
    $form["#attached"]["js"][] = drupal_get_path('module', 'base_min_max') . '/js/base_min_max.js';
  }
}

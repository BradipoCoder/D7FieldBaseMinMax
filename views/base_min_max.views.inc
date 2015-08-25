<?php
/**
 * Modify data for fields declared by this module
 * Implements hook_field_views_data_views_data_alter().
 */
function base_min_max_field_views_data_views_data_alter(&$data, $field) {
  $fieldName = $field["field_name"];
  $dataFieldName = 'field_data_' . $fieldName;
  if(isset($data[$dataFieldName])) {

    // Add filter for main Field
    $data[$dataFieldName][$fieldName]['filter'] = [
      'field' => $fieldName . '_base',
      'table' => $dataFieldName,
      'handler' => 'base_min_max_handler_filter_range',
      'additional fields' => [
        $fieldName . '_base',
        $fieldName . '_minimum',
        $fieldName . '_maximum',
      ],
      'field_name' => $fieldName,
      'allow empty' => true,
    ];

    // Remove filters from individual fields (Base, Minimum, Maximum)
    foreach($data[$dataFieldName] as $k => &$fieldData) {
      if($k != $fieldName && isset($fieldData['filter'])) {
        unset($fieldData['filter']);
      }
    }
  }
}


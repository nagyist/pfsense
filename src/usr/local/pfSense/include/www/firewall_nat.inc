<?php
/*
 * firewall_nat.inc
 *
 * part of pfSense (https://www.pfsense.org)
 * Copyright (c) 2014-2025 Rubicon Communications, LLC (Netgate)
 * All rights reserved.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

// Functions to support firewall_nat.php and firewall_nat_edit.php

require_once("config.gui.inc");
require_once("interfaces.inc");
require_once("util.inc");
require_once("pfsense-utils.inc");
require_once("ipsec.inc");
require_once("filter.inc");
require_once("itemid.inc");

/**
 * Add Port Forwarding rules.
 * @param array $rules_list A list of config rules.
 * @param int $index A config rule index used to place the new rule(s).
 * 		A zero or greater value adds the rule(s) after that index.
 * 		A value of -1 adds the rule(s) at the top before all separators.
 * 		A value of -2 or lower adds the rule(s) at the end after all
 * 		separators.
 * @param bool $replace If a rule is found where the new rule(s) would
 * 		be added that existing rule will be replaced with the new rule(s).
 */
function add_rdr_rules(array $rules_list, int $index = -2, bool $replace = false, array &$changes = []) {
	$index = get_anynat_rule_relative_index('rdr', $index);
	if (!isset($index)) {
		return;
	}
	$separators = config_get_path('nat/separator', []);
	$nat_rules = get_anynat_rules_map('rdr');
	$changed_rules = false;
	$changed_separators = false;
	foreach (get_anynat_rules_map('rdr', $rules_list) as $new_category_name => $new_category) {
		foreach ($new_category as $new_group_name => $new_group) {
			foreach ($new_group as $new_subcategory_name => $new_rules) {
				// Get existing rules to modify.
				$rules = array_get_path(
					$nat_rules,
					"{$new_category_name}/{$new_group_name}/{$new_subcategory_name}",
					[]
				);

				// Insert the rules.
				$result = insert_config_rules($rules, $separators, $new_rules, $index, $replace);
				if ($result['rules']) {
					array_set_path(
						$nat_rules,
						"{$new_category_name}/{$new_group_name}/{$new_subcategory_name}",
						$rules
					);
					$changed_rules = true;
					if ($result['separators']) {
						$changed_separators = true;
					}
				}
			}
		}
	}

	if ($changed_rules) {
		if ($changed_separators) {
			config_set_path('nat/separator', $separators);
			$changes['rdr_separators'] = true;
		}
		set_anynat_rules_map('rdr', $nat_rules);
		$changes['rdr_rules'] = true;

		// Check for filter rule associations.
		foreach ($rules_list as $new_rule) {
			if (isset($new_rule['associated-rule-id'])) {
				toggle_id($new_rule['associated-rule-id'], !isset($new_rule['disabled']), $changes);
			}
		}
	}
}

/**
 * Remove Port Forwarding rules.
 * @param int|array $search_index One or more indices of config rules.
 */
function remove_rdr_rules(int|array $search_index, array &$changes = []) {
	if (!is_array($search_index)) {
		$search_index = [$search_index];
	}
	if (empty($search_index)) {
		return;
	}

	$separators = config_get_path('nat/separator', []);
	$associated_id_list = [];
	$rules_map = get_anynat_rules_map('rdr');
	$changed_rules = false;
	$changed_separators = false;
	foreach (get_anynat_rules_index('rdr', $search_index) as $category_name => $category) {
		foreach ($category as $group_name => $group) {
			foreach ($group as $subcategory_name => $rules_index) {
				// Get existing rules to modify.
				$rules = &$rules_map[$category_name][$group_name][$subcategory_name];

				// Get related info before rules are removed.
				$relative_index_list = [];
				foreach (array_reverse($rules_index['matched_rules']) as $index) {
					$relative_index_list[] = $index['relative_index'];
					if (!empty($rules[$index['relative_index']]['associated-rule-id'])) {
						// Remove the association in NAT rules.
						$associated_id_list[] = $rules[$index['relative_index']]['associated-rule-id'];
					}
				}

				// Remove the rules.
				$result = remove_config_rules($rules, $separators, $relative_index_list);
				if ($result['rules']) {
					$changed_rules = true;
					if ($result['separators']) {
						$changed_separators = true;
					}
				}
				unset($rules);
			}
		}
	}

	if ($changed_rules) {
		if ($changed_separators) {
			config_set_path('nat/separator', $separators);
			$changes['rdr_separators'] = true;
		}
		set_anynat_rules_map('rdr', $rules_map);
		$changes['rdr_rules'] = true;
		foreach ($associated_id_list as $id) {
			delete_id($id, false, $changes);
		}
	}
}

$ifdisp = get_configured_interface_with_descr();

function build_localtype_list($json = false) {
	$list = get_specialnet('', [SPECIALNET_CHECKPERM, SPECIALNET_COMPAT_ADDRAL,
	                       SPECIALNET_IFADDR]);

	if ($json) {
		$rv = array();

		foreach ($list as $ifent => $ifname) {
			$rv[] = array("text" => $ifname, "value" => $ifent);
		}

		return(json_encode($rv));
	} else {
		return($list);
	}
}

function build_dsttype_list($json = false) {
	$list = get_specialnet('', [SPECIALNET_CHECKPERM, SPECIALNET_ANY, SPECIALNET_COMPAT_ADDRAL,
	                       SPECIALNET_NET, SPECIALNET_SELF, SPECIALNET_CLIENTS,
	                       SPECIALNET_IFADDR, SPECIALNET_IFNET, SPECIALNET_VIPS]);

	if ($json) {
		$rv = array();

		foreach ($list as $ifent => $ifname) {
			$rv[] = array("text" => $ifname, "value" => $ifent);
		}

		return(json_encode($rv));
	} else {
		return($list);
	}
}

function build_srctype_list($json = false) {
	$list = get_specialnet('', [SPECIALNET_CHECKPERM, SPECIALNET_ANY, SPECIALNET_COMPAT_ADDRAL,
	                       SPECIALNET_NET, SPECIALNET_CLIENTS, SPECIALNET_IFADDR,
	                       SPECIALNET_IFNET]);

	if ($json) {
		$rv = array();

		foreach ($list as $ifent => $ifname) {
			$rv[] = array("text" => $ifname, "value" => $ifent);
		}

		return(json_encode($rv));
	} else {
		return($list);
	}
}

function saveNATrule($post, $id, $json = false) {
	$rdr_lcltype_flags = [SPECIALNET_IFADDR];
	$rdr_srctype_flags = [SPECIALNET_CHECKPERM, SPECIALNET_ANY, SPECIALNET_CLIENTS, SPECIALNET_IFADDR, SPECIALNET_IFNET];
	$rdr_dsttype_flags = [SPECIALNET_ANY, SPECIALNET_SELF, SPECIALNET_CLIENTS, SPECIALNET_IFADDR, SPECIALNET_IFNET, SPECIALNET_VIPS];
	global $ifdisp;

	$a_nat = get_anynat_rules_list('rdr');

	$input_errors = array();

	if (isset($post['after'])) {
		$after = $post['after'];
	}

	if (array_key_exists(strtolower($post['proto']), get_ipprotocols('portsonly'))) {
		if ($post['srcbeginport_cust'] && !$post['srcbeginport']) {
			$post['srcbeginport'] = trim($post['srcbeginport_cust']);
		}
		if ($post['srcendport_cust'] && !$post['srcendport']) {
			$post['srcendport'] = trim($post['srcendport_cust']);
		}

		if ($post['srcbeginport'] == "any") {
			$post['srcbeginport'] = 0;
			$post['srcendport'] = 0;
		} else {
			if (!$post['srcendport']) {
				$post['srcendport'] = $post['srcbeginport'];
			}
		}
		if ($post['srcendport'] == "any") {
			$post['srcendport'] = $post['srcbeginport'];
		}

		if ($post['dstbeginport_cust'] && !$post['dstbeginport']) {
			$post['dstbeginport'] = trim($post['dstbeginport_cust']);
		}
		if ($post['dstendport_cust'] && !$post['dstendport']) {
			$post['dstendport'] = trim($post['dstendport_cust']);
		}

		if ($post['dstbeginport'] == "any") {
			$post['dstbeginport'] = "1";
			$post['dstendport'] = "65535";
			$post['localbeginport'] = "1";
		} else {
			if (!$post['dstendport']) {
				$post['dstendport'] = $post['dstbeginport'];
			}
		}
		if ($post['dstendport'] == "any") {
			$post['dstendport'] = $post['dstbeginport'];
		}

		if ($post['localbeginport_cust'] && !$post['localbeginport']) {
			$post['localbeginport'] = trim($post['localbeginport_cust']);
		}

		/* Make beginning port end port if not defined and endport is */
		if (!$post['srcbeginport'] && $post['srcendport']) {
			$post['srcbeginport'] = $post['srcendport'];
		}
		if (!$post['dstbeginport'] && $post['dstendport']) {
			$post['dstbeginport'] = $post['dstendport'];
		}
	} else {
		$post['srcbeginport'] = 0;
		$post['srcendport'] = 0;
		$post['dstbeginport'] = 0;
		$post['dstendport'] = 0;
	}

	if (get_specialnet($post['srctype'], $rdr_srctype_flags)) {
		$post['src'] = $post['srctype'];
		$post['srcmask'] = 0;
	} elseif ($post['srctype'] == "single") {
		if (is_ipaddrv6($post['src'])) {
			$post['srcmask'] = 128;
		} else {
			$post['srcmask'] = 32;
		}
	}

	if (get_specialnet($post['dsttype'], $rdr_dsttype_flags)) {
		$post['dst'] = $post['dsttype'];
		$post['dstmask'] = 0;
		// handle VIPs
		if (is_ipaddrv6($post['dsttype'])) {
			$post['dstmask'] = 128;
		} elseif (is_ipaddrv4($post['dsttype'])) {
			$post['dstmask'] = 32;
		}
	} elseif ($post['dsttype'] == "single") {
		if (is_ipaddrv6($post['dst'])) {
			$post['dstmask'] = 128;
		} else {
			$post['dstmask'] = 32;
		}
	} elseif (is_ipaddr($post['dsttype'])) {
		$post['dst'] = $post['dsttype'];
		$post['dsttype'] = "single";
		if (is_ipaddrv6($post['dst'])) {
			$post['dstmask'] = 128;
		} else {
			$post['dstmask'] = 32;
		}
	}

	if (get_specialnet($post['localtype'], $rdr_lcltype_flags)) {
		$post['localip'] = $post['localtype'];
	}

	$pconfig = $post;

	/* input validation */
	if (array_key_exists(strtolower($post['proto']), get_ipprotocols('portsonly'))) {
		$reqdfields = explode(" ", "interface proto dstbeginport dstendport");
		$reqdfieldsn = array(gettext("Interface"), gettext("Protocol"), gettext("Destination port from"), gettext("Destination port to"));
	} else {
		$reqdfields = explode(" ", "interface proto");
		$reqdfieldsn = array(gettext("Interface"), gettext("Protocol"));
	}

	if ($post['srctype'] == "single" || $post['srctype'] == "network") {
		$reqdfields[] = "src";
		$reqdfieldsn[] = gettext("Source address");
	}

	if ($post['dsttype'] == "single" || $post['dsttype'] == "network") {
		$reqdfields[] = "dst";
		$reqdfieldsn[] = gettext("Destination address");
	}

	if (!isset($post['nordr'])) {
		$reqdfields[] = "localip";
		$reqdfieldsn[] = gettext("Redirect target IP");
	}

	if (!$json) {
		do_input_validation($post, $reqdfields, $reqdfieldsn, $input_errors);
	}

	if (!$post['srcbeginport']) {
		$post['srcbeginport'] = 0;
		$post['srcendport'] = 0;
	}

	if (!$post['dstbeginport']) {
		$post['dstbeginport'] = 0;
		$post['dstendport'] = 0;
	}

	if ($post['src']) {
		$post['src'] = trim($post['src']);
	}

	if ($post['dst']) {
		$post['dst'] = trim($post['dst']);
	}

	if ($post['localip']) {
		$post['localip'] = trim($post['localip']);
	}

	if (!array_key_exists($post['interface'], create_interface_list($json))) {
		$input_errors[] = gettext("The submitted interface does not exist.");
	}

	if ((($post['natreflection'] == "purenat") || ($post['natreflection'] == "enable") ||
	    (($post['natreflection'] == "default") && !config_path_enabled('system', 'disablenatreflection'))) &&
	    ((get_specialnet($post['interface']) || is_interface_group($post['interface'])) &&
	    in_array($post['dst'], array('any', '0.0.0.0', '::')))) {
		$input_errors[] = gettext("The submitted interface does not support the 'Any' destination type with enabled NAT reflection.");
	}

	if (!isset($post['nordr']) && $post['localip'] &&
	    !is_ipaddroralias($post['localip']) && !get_specialnet($post['localtype'], $rdr_lcltype_flags)) {
		$input_errors[] = sprintf(gettext("\"%s\" is not a valid redirect target IP address or host alias."), $post['localip']);
	}

	if ($post['localip']) {
		if (get_specialnet($post['localtype'], $rdr_lcltype_flags)) {
			foreach ($ifdisp as $kif => $kdescr) {
				if ($post['localtype'] == "{$kif}ip") {
					if (($post['ipprotocol'] == 'inet') && !get_interface_ip($kif)) {
						$input_errors[] = sprintf(gettext("Redirect interface must have IPv4 address."));
						break;
					} elseif (($post['ipprotocol'] == 'inet6') && !get_interface_ipv6($kif)) {
						$input_errors[] = sprintf(gettext("Redirect interface must have IPv6 address."));
						break;
					}
				}
			}
		} elseif (($post['ipprotocol'] == 'inet') && is_ipaddrv6($post['localip'])) {
			$input_errors[] = sprintf(gettext("Redirect target IP must be IPv4."));
		} elseif (($post['ipprotocol'] == 'inet6') && is_ipaddrv4($post['localip'])) {
			$input_errors[] = sprintf(gettext("Redirect target IP must be IPv6."));
		}
	}

	if ($post['srcbeginport'] && !is_port_or_alias($post['srcbeginport'])) {
		$input_errors[] = sprintf(gettext("%s is not a valid start source port. It must be a port alias or integer between 1 and 65535."), $post['srcbeginport']);
	}
	if ($post['srcendport'] && !is_port_or_alias($post['srcendport'])) {
		$input_errors[] = sprintf(gettext("%s is not a valid end source port. It must be a port alias or integer between 1 and 65535."), $post['srcendport']);
	}
	if ($post['srcbeginport'] && $post['srcendport']) {
		if (is_port($post['srcbeginport']) && is_alias($post['srcendport']) ||
		    (is_alias($post['srcbeginport']) && is_port($post['srcendport']))) { 
			$input_errors[] = gettext("Source port range From/To values must a port number or alias, but not both.");
		} elseif (is_alias($post['srcbeginport']) && is_alias($post['srcendport']) &&
		    ($post['srcbeginport'] != $post['srcendport'])) {
			$input_errors[] = gettext("The 'From' alias of the Source port range must be equal to the 'To' alias.");
		}
	}
	if ($post['dstbeginport'] && !is_port_or_alias($post['dstbeginport'])) {
		$input_errors[] = sprintf(gettext("%s is not a valid start destination port. It must be a port alias or integer between 1 and 65535."), $post['dstbeginport']);
	}
	if ($post['dstendport'] && !is_port_or_alias($post['dstendport'])) {
		$input_errors[] = sprintf(gettext("%s is not a valid end destination port. It must be a port alias or integer between 1 and 65535."), $post['dstendport']);
	}
	if ($post['dstbeginport'] && $post['dstendport']) {
		if (is_port($post['dstbeginport']) && is_alias($post['dstendport']) ||
		    (is_alias($post['dstbeginport']) && is_port($post['dstendport']))) { 
			$input_errors[] = gettext("Destination port range From/To values must a port number or alias, but not both.");
		} elseif (is_alias($post['dstbeginport']) && is_alias($post['dstendport']) &&
		    ($post['dstbeginport'] != $post['dstendport'])) {
			$input_errors[] = gettext("The 'From' alias of the Destination port range must be equal to the 'To' alias.");
		}
	}

	if ((array_key_exists(strtolower($post['proto']), get_ipprotocols('portsonly'))) && (!isset($post['nordr']) && !is_port_or_alias($post['localbeginport']))) {
		$input_errors[] = sprintf(gettext("Redirect target port %s is not valid. It must be a port alias or integer between 1 and 65535."), $post['localbeginport']);
	}

	/* if user enters an alias and selects "network" then disallow. */
	if (($post['srctype'] == "network" && is_alias($post['src'])) ||
	    ($post['dsttype'] == "network" && is_alias($post['dst']))) {
		$input_errors[] = gettext("Alias entries must specify a single host or alias.");
	}

	if (!get_specialnet($post['srctype'], $rdr_srctype_flags)) {
		if (($post['src'] && !is_ipaddroralias($post['src']))) {
			$input_errors[] = sprintf(gettext("%s is not a valid source IP address or alias."), $post['src']);
		}
		if ($post['src']) {
			if (($post['ipprotocol'] == 'inet') && is_ipaddrv6($post['src'])) {
				$input_errors[] = sprintf(gettext("Source must be IPv4."));
			} elseif (($post['ipprotocol'] == 'inet6') && is_ipaddrv4($post['src'])) {
				$input_errors[] = sprintf(gettext("Source must be IPv6."));
			}
		}
		if (is_ipaddr($post['src']) && !is_subnet($post['src'] . '/' . $post['srcmask'])) {
			$input_errors[] = gettext("A valid source bit count must be specified.");
		}
	}

	if (!get_specialnet($post['dsttype'], $rdr_dsttype_flags)) {
		if (($post['dst'] && !is_ipaddroralias($post['dst']))) {
			$input_errors[] = sprintf(gettext("%s is not a valid destination IP address or alias."), $post['dst']);
		}
		if ($post['dst']) {
			if (($post['ipprotocol'] == 'inet') && is_ipaddrv6($post['dst'])) {
				$input_errors[] = sprintf(gettext("Destination must be IPv4."));
			} elseif (($post['ipprotocol'] == 'inet6') && is_ipaddrv4($post['dst'])) {
				$input_errors[] = sprintf(gettext("Destination must be IPv6."));
			}
		}
		if (is_ipaddr($post['dst']) && !is_subnet($post['dst'] . '/' . $post['dstmask'])) {
			$input_errors[] = gettext("A valid destination bit count must be specified.");
		}
	}

	if ($post['srcbeginport'] > $post['srcendport']) {
		/* swap */
		$tmp = $post['srcendport'];
		$post['srcendport'] = $post['srcbeginport'];
		$post['srcbeginport'] = $tmp;
	}

	if ($post['dstbeginport'] > $post['dstendport']) {
		/* swap */
		$tmp = $post['dstendport'];
		$post['dstendport'] = $post['dstbeginport'];
		$post['dstbeginport'] = $tmp;
	}

	if (!$input_errors) {
		if (!isset($post['nordr']) && ((int) $post['dstendport'] - (int) $post['dstbeginport'] + (int) $post['localbeginport']) > 65535) {
			$input_errors[] = gettext("The target port range must be an integer between 1 and 65535.");
		}
	}

	/* check for overlaps */
	foreach ($a_nat as $natent) {
		if (isset($id) && ($a_nat[$id]) && ($a_nat[$id] === $natent)) {
			continue;
		}
		if ($natent['ipprotocol'] != $post['ipprotocol']) {
			continue;
		}
		if ($natent['interface'] != $post['interface']) {
			continue;
		}
 		if ((($post['srctype'] == "network") && ($natent['source']['address'] != ($post['src'] . '/' . $post['srcmask']))) ||
		    (($post['srctype'] == "single") && ($natent['source']['address'] != $post['src'])) ||
		    ((get_specialnet($post['srctype'], $rdr_srctype_flags)) && ($natent['source']['network'] != $post['src']))) {
			continue;
		}
		if ((($post['dsttype'] == "network") && ($natent['destination']['address'] != ($post['dst'] . '/' . $post['dstmask']))) ||
		    (($post['dsttype'] == "single") && ($natent['destination']['address'] != $post['dst'])) ||
		    ((get_specialnet($post['dsttype'], $rdr_dsttype_flags)) && ($natent['destination']['network'] != $post['dst']))) {
			continue;
		}
		if (($natent['protocol'] != $post['proto']) &&
		    (strtoupper($natent['protocol']) != "TCP/UDP") && (strtoupper($post['proto']) != "TCP/UDP")) {
			continue;
		}

		list($begp, $endp) = explode("-", $natent['destination']['port']);
		if (!$endp) {
			$endp = $begp;
		}

		if (!((($post['dstbeginport'] < $begp) && ($post['dstendport'] < $begp)) ||
		      (($post['dstbeginport'] > $endp) && ($post['dstendport'] > $endp)))) {
			$input_errors[] = gettext("The destination port range overlaps with an existing entry.");
			break;
		}

	}

	/* https://redmine.pfsense.org/issues/12319 */
	if (($post['ipprotocol'] == 'inet6') && (($post['natreflection'] == 'enable') ||
	    (($post['natreflection'] == 'default') && !config_path_enabled('system', 'disablenatreflection') &&
	    !config_path_enabled('system', 'enablenatreflectionpurenat')))) {
		$input_errors[] = gettext("IPv6 address family doesn't support NAT + Proxy reflection mode.");
	}

	if (!$input_errors) {

		$natent = array();

		if (isset($post['disabled'])) {
			$natent['disabled'] = true;
		}

		if (isset($post['nordr'])) {
			$natent['nordr'] = true;
		}

		if ($natent['nordr']) {
			$post['associated-rule-id'] = '';
			$post['filter-rule-association'] = '';
		}

		pconfig_to_address($natent['source'], $post['src'],
			$post['srcmask'], $post['srcnot'],
			$post['srcbeginport'], $post['srcendport'], false, $rdr_srctype_flags);

		pconfig_to_address($natent['destination'], $post['dst'],
			$post['dstmask'], $post['dstnot'],
			$post['dstbeginport'], $post['dstendport'], false, array_diff($rdr_dsttype_flags, [SPECIALNET_VIPS]));

		$natent['ipprotocol'] = $post['ipprotocol'];
		$natent['protocol'] = $post['proto'];

		if (!isset($natent['nordr'])) {
			$natent['target'] = $post['localip'];
			if (array_key_exists(strtolower($natent['protocol']), get_ipprotocols('portsonly'))) {
				$natent['local-port'] = $post['localbeginport'];
			}
		}

		$natent['interface'] = $post['interface'];
		$natent['descr'] = $post['descr'];
		$natent['associated-rule-id'] = $post['associated-rule-id'];

		if ($post['filter-rule-association'] == "pass") {
			$natent['associated-rule-id'] = "pass";
		}

		if ($post['nosync'] == "yes") {
			$natent['nosync'] = true;
		} else {
			unset($natent['nosync']);
		}

		if ($post['natreflection'] == "enable" || $post['natreflection'] == "purenat" || $post['natreflection'] == "disable") {
			$natent['natreflection'] = $post['natreflection'];
		} else {
			unset($natent['natreflection']);
		}

		// If we used to have an associated filter rule, but no-longer should have one
		if (!empty($a_nat[$id]) && (empty($natent['associated-rule-id']) || $natent['associated-rule-id'] != $a_nat[$id]['associated-rule-id'])) {
			// Delete the previous rule
			delete_id($a_nat[$id]['associated-rule-id'], false);
			if (!$json) {
				mark_subsystem_dirty('filter');
			}
		}

		$need_filter_rule = false;
		// Updating a rule with a filter rule associated
		if (!empty($natent['associated-rule-id'])) {
			$need_filter_rule = true;
		}
		// Create a rule or if we want to create a new one
		if ($natent['associated-rule-id'] == 'new') {
			$need_filter_rule = true;
			unset($natent['associated-rule-id']);
			$post['filter-rule-association']='add-associated';
		}
		// If creating a new rule, where we want to add the filter rule, associated or not
		else if (isset($post['filter-rule-association']) &&
		    ($post['filter-rule-association'] == 'add-associated' ||
		     $post['filter-rule-association'] == 'add-unassociated')) {
			$need_filter_rule = true;
		}

		if ($need_filter_rule == true) {
			$filter_rule_config = get_filter_rules_list();
			/* auto-generate a matching firewall rule */
			$filterent = array();
			unset($filterentid);

			// If a rule already exists, load it
			if (!empty($natent['associated-rule-id'])) {
				$filterentid = get_id($natent['associated-rule-id'], $filter_rule_config);
				if ($filterentid === false) {
					$filterent['associated-rule-id'] = $natent['associated-rule-id'];
				} else {
					$filterent = $filter_rule_config[$filterentid];
				}
			}

			pconfig_to_address($filterent['source'], $post['src'],
				$post['srcmask'], $post['srcnot'],
				$post['srcbeginport'], $post['srcendport'], false, $rdr_srctype_flags);

			// Update interface, protocol and destination
			$filterent['interface'] = $post['interface'];
			$filterent['ipprotocol'] = $post['ipprotocol'];
			$filterent['protocol'] = $post['proto'];
			if (get_specialnet($post['localtype'], $rdr_lcltype_flags)) {
				$filterent['destination']['network'] = $post['localtype'];
				if (isset($filterent['destination']['address'])) {
					unset($filterent['destination']['address']);
				}
			} else {
				$filterent['destination']['address'] = $post['localip'];
				if (isset($filterent['destination']['network'])) {
					unset($filterent['destination']['network']);
				}
			}

			if (isset($post['disabled'])) {
				$filterent['disabled'] = true;
			}

			$dstpfrom = $post['localbeginport'];
			
			/*
			 * See Redmine #13601
			 * If $dstpfrom is an alias, we want $dstpto to be an alias as well... 
			 */
			$dstpto = is_alias($dstpfrom) ? $dstpfrom : ((int) $dstpfrom + (int) $post['dstendport'] - (int) $post['dstbeginport']);

			if ($dstpfrom == $dstpto) {
				$filterent['destination']['port'] = $dstpfrom;
			} elseif (is_port($dstpfrom) && is_port($dstpto)) {
				$filterent['destination']['port'] = $dstpfrom . "-" . $dstpto;
			}

			/*
			 * Our firewall filter description may be no longer than
			 * 63 characters, so don't let it be.
			 */
			$filterent['descr'] = substr("NAT " . $post['descr'], 0, 62);

			// If this is a new rule, create an ID and add the rule
			if ($post['filter-rule-association'] == 'add-associated') {
				$filterent['associated-rule-id'] = $natent['associated-rule-id'] = get_unique_id();
				if ($natent['nosync']) {
					$filterent['nosync'] = true;
				}
				$filterent['tracker'] = (int)microtime(true);
				$filterent['created'] = make_config_revision_entry(null, gettext("NAT Port Forward"));
				add_filter_rules([$filterent]);
			} elseif ($filterentid !== false) {
				add_filter_rules([$filterent], $filterentid, true);
			}

			if (!$json) {
				mark_subsystem_dirty('filter');
			}
		}

		if (isset($a_nat[$id]['created']) && is_array($a_nat[$id]['created'])) {
			$natent['created'] = $a_nat[$id]['created'];
		}

		$natent['updated'] = make_config_revision_entry();

		if (!$json) {
			// Allow extending of the firewall edit page and include custom input validation
			pfSense_handle_custom_code("/usr/local/pkg/firewall_nat/pre_write_config");
		}

		// Update the NAT entry now
		$changes = [];
		if (isset($id) && $a_nat[$id]) {
			if (isset($a_nat[$id]['labels'])) {
				// Keep rule labels.
				$natent['labels'] = $a_nat[$id]['labels'];
			}
			if (isset($a_nat[$id]['force_category'])) {
				// Keep filter category override.
				$natent['force_category'] = $a_nat[$id]['force_category'];
			}
			add_rdr_rules([$natent], $id, true, $changes);
		} else {
			$natent['created'] = make_config_revision_entry();
			if (is_numeric($after)) {
				add_rdr_rules([$natent], $after, false, $changes);
			} else {
				add_rdr_rules([$natent], -2, false, $changes);
			}
		}

		if (write_config(gettext("Firewall: NAT: Port Forward - saved/edited a port forward rule."))) {
			if (!$json && isset($changes['filter_rules'])) {
				mark_subsystem_dirty('filter');
			}
			if (!$json) {
				mark_subsystem_dirty('natconf');
			} else {
				filter_configure();
			}
		}
	}

	$rv = array();
	$rv['input_errors'] = $input_errors;
	$rv['pconfig'] = $pconfig;

	return $json ? json_encode($rv) : $rv;
}

function toggleNATrule($post, $json = false) {
	// Check for single rule
	if (!(is_array($post['rule']) && count($post['rule']))) {
		$post['rule'] = [ $post['id'] ];
	}

	$changes = [];
	foreach ($post['rule'] as $rulei) {
		if (config_path_enabled("nat/rule/{$rulei}", 'disabled')) {
			config_del_path("nat/rule/{$rulei}/disabled");
			$rule_status = true;
		} else {
			config_set_path("nat/rule/{$rulei}/disabled", true);
			$rule_status = false;
		}

		// Check for filter rule associations
		$associated_rule_id = config_get_path("nat/rule/{$rulei}/associated-rule-id");

		if ($associated_rule_id != null) {
			toggle_id($associated_rule_id, $rule_status, $changes);
			unset($rule_status);
		}
	}

	if (count($post['rule']) == 1) {
		$action = config_path_enabled("nat/rule/{$post['rule'][0]}", "disabled") ? "disabled":"enabled";
		$write_ret = write_config(gettext("Firewall: NAT: Port forward - {$action} a NAT rule"));
	} else if (count($post['rule']) > 1) {
		$write_ret = write_config(gettext("Firewall: NAT: Port forward - enable/disable for selected NAT rules"));
	}

	if ($write_ret) {
		if (!$json) {
			mark_subsystem_dirty('natconf');
			if (isset($changes['filter_rules'])) {
				mark_subsystem_dirty('filter');
			}
			header("Location: firewall_nat.php");
			exit;
		} else {
			if (isset($post['id'])) {
				return $action;
			} else {
				filter_configure();
			}
		}
	}
}

function deleteNATrule($post, $json = false) {
	// Check for single rule
	if (!(is_array($post['rule']) && count($post['rule']))) {
		$post['rule'] = array( $post['id'] => $post['id'] );
	}

	$changes = [];
	remove_rdr_rules($post['rule'], $changes);

	if (isset($changes['rdr_rules'])) {
		$write_ret = write_config("Firewall: NAT: Port forward - rule(s) deleted");

		if ($write_ret) {
			if ($json) {
				filter_configure();
			} else {
				mark_subsystem_dirty('natconf');
				if (isset($changes['filter_rules'])) {
					mark_subsystem_dirty('filter');
				}
			}
		}
	}

	if(!$json) {
		header("Location: firewall_nat.php");
		exit;
	}
}

function applyNATrules() {
	$retval = 0;

	$retval |= filter_configure();

	pfSense_handle_custom_code("/usr/local/pkg/firewall_nat/apply");

	if ($retval == 0) {
		clear_subsystem_dirty('natconf');
		clear_subsystem_dirty('filter');
	}

	return $retval;
}

// Re-order the NAT rules per the array of indices passed in $post
function reorderNATrules($post, $json = false) {
	$updated = false;
	$dirty = false;

	$a_separators = config_get_path('nat/separator', []);

	/* update rule order, POST[rule] is an array of ordered IDs */
	if (is_array($post['rule']) && !empty($post['rule'])) {
		if (set_anynat_rules_order('rdr', $post['rule'])) {
			$dirty = true;
		}
	}

	/* update separator order, POST[separator] is an array of ordered IDs */
	if (is_array($post['separator']) && !empty($post['separator'])) {
		$new_separator = array();
		$idx = 0;

		foreach ($post['separator'] as $separator) {
			$new_separator['sep' . $idx++] = $separator;
		}

		if ($a_separators !== $new_separator) {
			$a_separators = $new_separator;
			$updated = true;
		}
	} else if (!empty($a_separators)) {
		$a_separators = "";
		$updated = true;
	}

	if ($updated || $dirty) {
		config_set_path('nat/separator', $a_separators);
		if (write_config("NAT: Rule order changed")) {
			if ($json) {
				filter_configure();
			} else if ($dirty) {
				mark_subsystem_dirty('natconf');
			}
		}
	}

	if(!$json) {
		header("Location: firewall_nat.php");
		exit;
	}
}

function MVC_filter_get_interface_list() {
	$iflist = MVC_create_interface_list();
	$filter_ifs = array();

	foreach ($iflist as $ifent => $ifname) {
		$filter_ifs[] = array("text" => $ifname, "value" => $ifent);
	}

	return json_encode($filter_ifs);
}

function MVC_create_interface_list() {
	$iflist = array();

	// add group interfaces
	if (is_array(config_get_path('ifgroups/ifgroupentry'))) {
		foreach (config_get_path('ifgroups/ifgroupentry', []) as $ifgen) {
			$iflist[$ifgen['ifname']] = $ifgen['ifname'];
		}
	}

	foreach (get_configured_interface_with_descr() as $ifent => $ifdesc) {
		$iflist[$ifent] = $ifdesc;
	}

	if (config_get_path('l2tp/mode') == "server") {
		$iflist['l2tp'] = gettext('L2TP VPN');
	}

	if (is_pppoe_server_enabled()) {
		$iflist['pppoe'] = gettext("PPPoE Server");
	}

	// add ipsec interfaces
	if (ipsec_enabled()) {
		$iflist["enc0"] = gettext("IPsec");
	}

	// add openvpn/tun interfaces
	if (config_get_path('openvpn/openvpn-server') || config_get_path('openvpn/openvpn-client')) {
		$iflist["openvpn"] = gettext("OpenVPN");
	}

	return($iflist);
}

function getNATRule($id, $json = false) {
	$a_nat = get_anynat_rules_list('rdr');

	if (isset($id) && $a_nat[$id]) {
		if (isset($a_nat[$id]['created']) && is_array($a_nat[$id]['created'])) {
			$pconfig['created'] = $a_nat[$id]['created'];
		}

		if (isset($a_nat[$id]['updated']) && is_array($a_nat[$id]['updated'])) {
			$pconfig['updated'] = $a_nat[$id]['updated'];
		}

		$pconfig['disabled'] = isset($a_nat[$id]['disabled']);
		$pconfig['nordr'] = isset($a_nat[$id]['nordr']);

		address_to_pconfig($a_nat[$id]['source'], $pconfig['src'],
			$pconfig['srcmask'], $pconfig['srcnot'],
			$pconfig['srcbeginport'], $pconfig['srcendport']);

		address_to_pconfig($a_nat[$id]['destination'], $pconfig['dst'],
			$pconfig['dstmask'], $pconfig['dstnot'],
			$pconfig['dstbeginport'], $pconfig['dstendport']);

		if (($pconfig['dstbeginport'] == 1) && ($pconfig['dstendport'] == 65535)) {
			$pconfig['dstbeginport'] = "any";
			$pconfig['dstendport'] = "any";
		}

		$pconfig['ipprotocol'] = $a_nat[$id]['ipprotocol'];
		$pconfig['proto'] = $a_nat[$id]['protocol'];
		$pconfig['localip'] = $a_nat[$id]['target'];
		$pconfig['localbeginport'] = $a_nat[$id]['local-port'];
		$pconfig['descr'] = $a_nat[$id]['descr'];
		$pconfig['interface'] = $a_nat[$id]['interface'];
		$pconfig['associated-rule-id'] = $a_nat[$id]['associated-rule-id'];
		$pconfig['nosync'] = isset($a_nat[$id]['nosync']);
		$pconfig['natreflection'] = $a_nat[$id]['natreflection'];

		if (!$pconfig['interface']) {
			$pconfig['interface'] = "wan";
		}
	} else {
		$pconfig['interface'] = "wan";
		$pconfig['src'] = "any";
		$pconfig['srcbeginport'] = "any";
		$pconfig['srcendport'] = "any";
	}

	return $json ? json_encode($pconfig):$pconfig;
}
?>

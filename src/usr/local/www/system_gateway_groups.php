<?php
/*
 * system_gateway_groups.php
 *
 * part of pfSense (https://www.pfsense.org)
 * Copyright (c) 2004-2013 BSD Perimeter
 * Copyright (c) 2013-2016 Electric Sheep Fencing
 * Copyright (c) 2014-2025 Rubicon Communications, LLC (Netgate)
 * Copyright (c) 2010 Seth Mos <seth.mos@dds.nl>
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

##|+PRIV
##|*IDENT=page-system-gatewaygroups
##|*NAME=System: Gateway Groups
##|*DESCR=Allow access to the 'System: Gateway Groups' page.
##|*MATCH=system_gateway_groups.php*
##|-PRIV

require_once("guiconfig.inc");
require_once("functions.inc");
require_once("filter.inc");
require_once("shaper.inc");
require_once("openvpn.inc");

$a_gateways = config_get_path('gateways/gateway_item', []);
$changedesc = gettext("Gateway Groups") . ": ";


$pconfig = $_REQUEST;

if ($_POST['apply']) {

	$retval = 0;

	$retval |= system_routing_configure();
	send_multiple_events(array("service reload dyndnsall", "service reload ipsecdns", "filter reload"));

	/* reconfigure our gateway monitor */
	setup_gateways_monitor();

	if ($retval == 0) {
		clear_subsystem_dirty('staticroutes');
	}

	foreach (config_get_path('gateways/gateway_group', []) as $gateway_group) {
		$gw_subsystem = 'gwgroup.' . $gateway_group['name'];
		if (is_subsystem_dirty($gw_subsystem)) {
			openvpn_resync_gwgroup($gateway_group['name']);
			clear_subsystem_dirty($gw_subsystem);
		}
	}
}

$a_gateway_groups = config_get_path('gateways/gateway_group', []);
if (($_POST['act'] == "del") && $a_gateway_groups[$_POST['id']]) {
	if ((config_get_path('gateways/defaultgw4', '') == $a_gateway_groups[$_POST['id']]['name']) ||
	    (config_get_path('gateways/defaultgw6', '') == $a_gateway_groups[$_POST['id']]['name'])) {
		$input_errors[] = gettext('Cannot remove a gateway group that is being used as the default gateway.');
	} else {
		$changedesc .= sprintf(gettext("removed gateway group %s"), $_POST['id']);
		foreach (get_filter_rules_list() as $idx => $rule) {
			if ($rule['gateway'] == $a_gateway_groups[$_REQUEST['id']]['name']) {
				config_del_path("filter/rule/{$idx}/gateway");
			}
		}

		config_del_path("gateways/gateway_group/{$_POST['id']}");
		write_config($changedesc);
		mark_subsystem_dirty('staticroutes');
		header("Location: system_gateway_groups.php");
		exit;
	}
}

function gateway_exists($gwname) {
	$gateways = get_gateways();

	if (is_array($gateways)) {
		foreach ($gateways as $gw) {
			if ($gw['name'] == $gwname) {
				return(true);
			}
		}
	}

	return(false);
}

$pgtitle = array(gettext("System"), gettext("Routing"), gettext("Gateway Groups"));
$pglinks = array("", "system_gateways.php", "@self");
$shortcut_section = "gateway-groups";

include("head.inc");

if ($_POST['apply']) {
	print_apply_result_box($retval);
}

if (is_subsystem_dirty('staticroutes')) {
	print_apply_box(gettext("The gateway configuration has been changed.") . "<br />" . gettext("The changes must be applied for them to take effect."));
}

if ($input_errors) {
	print_input_errors($input_errors);
}

$tab_array = array();
$tab_array[] = array(gettext("Gateways"), false, "system_gateways.php");
$tab_array[] = array(gettext("Static Routes"), false, "system_routes.php");
$tab_array[] = array(gettext("Gateway Groups"), true, "system_gateway_groups.php");
display_top_tabs($tab_array);
?>
<div class="panel panel-default">
	<div class="panel-heading"><h2 class="panel-title"><?=gettext('Gateway Groups')?></h2></div>
	<div class="panel-body">
		<div class="table-responsive">
			<table class="table table-striped table-hover table-condensed table-rowdblclickedit">
				<thead>
					<tr>
						<th><?=gettext("Group Name")?></th>
						<th><?=gettext("Gateways")?></th>
						<th><?=gettext("Priority")?></th>
						<th><?=gettext("Description")?></th>
						<th><?=gettext("Actions")?></th>
					</tr>
				</thead>
				<tbody>
<?php
$i = 0;
foreach (config_get_path('gateways/gateway_group', []) as $gateway_group):
?>
					<tr>
						<td>
						   <?=$gateway_group['name']?>
						</td>
						<td>
<?php
	foreach ($gateway_group['item'] as $item) {
		$itemsplit = explode("|", $item);
		if (gateway_exists($itemsplit[0])) {
			print(htmlspecialchars(strtoupper($itemsplit[0])) . "<br />\n");
		}
	}
?>
						</td>
						<td>
<?php
	foreach ($gateway_group['item'] as $item) {
		$itemsplit = explode("|", $item);
		if (gateway_exists($itemsplit[0])) {
			print("Tier ". htmlspecialchars($itemsplit[1]) . "<br />\n");
		}
	}
?>
						</td>
						<td>
							<?=htmlspecialchars($gateway_group['descr'])?>
						</td>
						<td>
							<a href="system_gateway_groups_edit.php?id=<?=$i?>" class="fa-solid fa-pencil" title="<?=gettext('Edit gateway group')?>"></a>
							<a href="system_gateway_groups_edit.php?dup=<?=$i?>" class="fa-regular fa-clone" title="<?=gettext('Copy gateway group')?>"></a>
							<a href="system_gateway_groups.php?act=del&amp;id=<?=$i?>" class="fa-solid fa-trash-can" title="<?=gettext('Delete gateway group')?>" usepost></a>
						</td>
					</tr>
<?php
	$i++;
endforeach;
?>
				</tbody>
			</table>
		</div>
	</div>
</div>

<nav class="action-buttons">
	<a href="system_gateway_groups_edit.php" class="btn btn-success btn-sm">
		<i class="fa-solid fa-plus icon-embed-btn"></i>
		<?=gettext('Add')?>
	</a>
</nav>

<div class="infoblock">
	<?php print_info_box(sprintf(gettext('Remember to use these Gateway Groups in firewall rules in order to enable load balancing, failover, ' .
						   'or policy-based routing.%1$s' .
						   'Without rules directing traffic into the Gateway Groups, they will not be used.'), '<br />'), 'info', false); ?>
</div>
<?php
include("foot.inc");

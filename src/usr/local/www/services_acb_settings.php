<?php
/*
 * services_acb_settings.php
 *
 * part of pfSense (https://www.pfsense.org)
 * Copyright (c) 2004-2013 BSD Perimeter
 * Copyright (c) 2013-2016 Electric Sheep Fencing
 * Copyright (c) 2014-2025 Rubicon Communications, LLC (Netgate)
 * All rights reserved.
 *
 * originally based on m0n0wall (http://m0n0.ch/wall)
 * Copyright (c) 2003-2004 Manuel Kasper <mk@neon1.net>.
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
##|*IDENT=page-services-acb-settings
##|*NAME=Services: Auto Config Backup: Settings
##|*DESCR=Configure the auto config backup system.
##|*MATCH=services_acb_settings.php*
##|-PRIV

require_once("guiconfig.inc");
require_once("functions.inc");
require_once("pfsense-utils.inc");
require_once("services.inc");
require_once("acb.inc");

$pconfig = config_get_path('system/acb', []);

if (isset($_POST['save'])) {
	unset($input_errors);
	$pconfig = $_POST;

	/* Input Validation */
	$reqdfields = explode(" ", "encryption_password");
	$reqdfieldsn = array(gettext("Encryption password"));
	do_input_validation($_POST, $reqdfields, $reqdfieldsn, $input_errors);

	/* Check Enable */
	if (!empty($_POST['enable']) &&
	    ($_POST['enable'] != 'yes')) {
		$input_errors[] = gettext("Invalid Enable value.");
	}

	/* Check Encryption Password */
	if (strlen($_POST['encryption_password']) < 8) {
		$input_errors[] = gettext("Encryption Password must contain at least 8 characters");
	}
	$update_ep = false;
	if ($_POST['encryption_password'] != "********") {
		if ($_POST['encryption_password'] != $_POST['encryption_password_confirm']) {
			$input_errors[] = gettext("Encryption Password and confirmation value do not match");
		} else {
			$update_ep = true;
		}
	}

	/* Check Backup Frequency */
	if (!in_array($_POST['frequency'], ['cron', 'every'])) {
		$input_errors[] = gettext("Invalid frequency value");
	}

	if ($_POST['frequency'] === 'cron') {
		if (!preg_match('/^[0-9\*\/\-\,]+$/', $_POST['minute'] . $_POST['hour'] . $_POST['day'] . $_POST['month'] . $_POST['dow']))  {
			$input_errors[] = gettext("Schedule values may only contain the following characters: 0-9 - , / *");
		}
	}

	/* Check Hint/Identifier */
	if (!empty($_POST['hint']) &&
	    (strlen($_POST['hint']) > 255)) {
		$input_errors[] = gettext("Hint/Identifier must be less than 255 characters in length.");
	}

	/* Check Manual backup limit */
	if (!empty($_POST['numman']) &&
	    !is_numericint($_POST['numman'])) {
		$input_errors[] = gettext("Manual Backup Limit must be blank or an integer.");
	}
	if ((int)$_POST['numman'] < 0) {
		$input_errors[] = gettext("Manual Backup Limit cannot be negative.");
	}
	if ((int)$_POST['numman'] > 50) {
		$input_errors[] = gettext("Manual Backup Limit cannot be larger than 50.");
	}

	/* Check Descending Date Order */
	if (!empty($_POST['reverse']) &&
	    ($_POST['reverse'] != 'yes')) {
		$input_errors[] = gettext("Invalid Descending Date Order value.");
	}

	/* Store updated ACB configuration */
	if (!$input_errors) {
		$pconfig = setup_ACB(
			$pconfig['enable'],
			$pconfig['hint'],
			$pconfig['frequency'],
			$pconfig['minute'],
			$pconfig['hour'],
			$pconfig['month'],
			$pconfig['day'],
			$pconfig['dow'],
			$pconfig['numman'],
			$pconfig['reverse'],
			($update_ep ? $pconfig['encryption_password'] : "")
		);
	}
}

$pgtitle = array(gettext("Services"), gettext("Auto Configuration Backup"), gettext("Settings"));
include("head.inc");

if ($input_errors) {
	print_input_errors($input_errors);
}

$tab_array = array();
$tab_array[] = array("Settings", true, "/services_acb_settings.php");
$tab_array[] = array("Restore", false, "/services_acb.php");
$tab_array[] = array("Backup Now", false, "/services_acb_backup.php");
display_top_tabs($tab_array);

$form = new Form;
$section = new Form_Section('Auto Config Backup');

$section->addInput(new Form_Checkbox(
	'enable',
	'Enable ACB',
	'Enable automatic configuration backups',
	($pconfig['enable'] == "yes")
))->setHelp("Auto Configuration Backup automatically encrypts configuration backup content using the " .
	    "Encryption Password below and then securely uploads the encrypted backup over HTTPS to Netgate servers.");

$group = new Form_MultiCheckboxGroup('Backup Frequency');

$group->add(new Form_MultiCheckbox(
	'frequency',
	'',
	'Automatically backup on every configuration change',
	(!isset($pconfig['frequency']) || $pconfig['frequency'] === 'every'),
	'every'
))->displayasRadio();

$group->add(new Form_MultiCheckbox(
	'frequency',
	'',
	'Automatically backup on a regular schedule',
	($pconfig['frequency'] === 'cron'),
	'cron'
))->displayasRadio();

$group->addClass("notoggleall");
$section->add($group);

$group = new Form_Group("Schedule");

$group->add(new Form_Input(
	'minute',
	'Minute',
	'text',
	(isset($pconfig['minute']) ? $pconfig['minute'] : strval(random_int(0,59)))
))->setHelp("Minute (0-59)");

$group->add(new Form_Input(
	'hour',
	'Hour',
	'text',
	(isset($pconfig['hour']) ? $pconfig['hour']:'0')
))->setHelp("Hours (0-23)");

$group->add(new Form_Input(
	'day',
	'Day of month',
	'text',
	(isset($pconfig['day']) ? $pconfig['day']:'*')
))->setHelp("Day (1-31)");

$group->add(new Form_Input(
	'month',
	'Month',
	'text',
	(isset($pconfig['month']) ? $pconfig['month']:'*')
))->setHelp("Month (1-12)");

$group->add(new Form_Input(
	'dow',
	'Day of week',
	'text',
	(isset($pconfig['dow']) ? $pconfig['dow']:'*')
))->setHelp("Day of week (0-6)");

$group->addClass("cronsched");
$group->setHelp(sprintf('Use * ("every"), divisor, or exact value.  Minutes are randomly chosen by default. See %s for more information.',
	'<a href="https://www.freebsd.org/cgi/man.cgi?crontab(5)" target="_blank">Cron format</a>'));
$section->add($group);

$group = new Form_Group("Device Key");

$userkey = get_acb_device_key();
$legacy_key = get_acb_legacy_device_key();
$legacy_string = "";
if (is_valid_acb_device_key($legacy_key) &&
    ($legacy_key == $userkey)) {
	$legacy_string = sprintf(gettext('%1$s%1$sThis is a legacy style key derived from the SSH public key.%1$s' .
				'The best practice is to change this key to a randomized key using the ' .
				'%2$sChange Key%3$s button.'),
				'<br/>', '<strong>', '</strong>');
}

$group->add(new Form_Input(
	'devkey',
	'Device Key',
	'text',
	$userkey
))->setWidth(7)->setReadonly()->setHelp('Unique key which identifies backups associated with this device.%1$s%1$s' .
	'%2$sKeep a secure copy of this value!%3$s %4$s%1$s' .
	'If this key is lost, all backups for this device will be lost!%5$s',
	'<br/>', '<strong>', '</strong>', acb_key_download_link('device', $userkey), $legacy_string);

$group->add(new Form_Button(
	'changekey',
	'Change Key',
	null,
	'fa-solid fa-key'
))->addClass('btn-info btn-xs');

$section->add($group);

$section->addPassword(new Form_Input(
	'encryption_password',
	'*Encryption Password',
	'password',
	$pconfig['encryption_password']
))->setHelp('AutoConfigBackup uses this string to encrypt the contents of the backup before upload. ' .
	'The best practice for security is to use a long and complex string.%1$s%1$s' .
	'%2$sKeep a secure copy of this value!%3$s%1$s' .
	'If this password is lost, any backups encrypted with this string will be unreadable!',
	'<br/>', '<strong>', '</strong>');

$section->addInput(new Form_Input(
	'hint',
	'Hint/Identifier',
	'text',
	$pconfig['hint']
))->setHelp("Optional identifier which AutoConfigBackup will store in plain text along with each encrypted backup. " .
	"This may allow Netgate TAC to recover a device key should it become lost.");

$section->addInput(new Form_Input(
	'numman',
	'Manual Backup Limit',
	'number',
	$pconfig['numman'],
	['min'=>'0', 'max'=>'50']
))->setHelp("Number of manual backup entries AutoConfigBackup will retain on the server, " .
	"which will not be overwritten by automatic backups. " .
	"The maximum value is 50 retained manual backups out of the 100 total entries.");

$section->addInput(new Form_Checkbox(
	'reverse',
	'Descending Date Order',
	'List backups in descending order by revision date/time',
	($pconfig['reverse'] == "yes")
))->setHelp("List backups in descending order (newest first) when viewing the restore section.");

$form->add($section);

print $form;

?>
</div>

<script type="text/javascript">
//<![CDATA[
events.push(function() {
	$('input:radio[name=frequency]').click(function() {
		hideClass("cronsched", ($(this).val() != 'cron'));
	});

	hideClass("cronsched", ("<?=htmlspecialchars($pconfig['frequency'])?>" != 'cron'));

	$('#changekey').click(function(event) {
		event.preventDefault();
		$(location).prop('href', "/services_acb_changekey.php");
	});

});
//]]>
</script>

<?php
include("foot.inc");
?>

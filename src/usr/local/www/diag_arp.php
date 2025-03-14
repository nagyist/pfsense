<?php
/*
* diag_arp.php
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
##|*IDENT=page-diagnostics-arptable
##|*NAME=Diagnostics: ARP Table
##|*DESCR=Allow access to the 'Diagnostics: ARP Table' page.
##|*MATCH=diag_arp.php*
##|-PRIV

@ini_set('zlib.output_compression', 0);
@ini_set('implicit_flush', 1);

require_once('guiconfig.inc');
require_once('diag_arp.inc');

define('ARP_BIN', '/usr/sbin/arp');

/* delete ARP cache entry */
if (isset($_POST['deleteentry'])) {
	$ip = $_POST['deleteentry'];

	$rc = 0;
	$savemsg = sprintf(gettext('The ARP cache entry for %s as been deleted.'), $ip);
	$savemsgtype = 'success';

	if (is_ipaddrv4($ip)) {
		exec(implode(' ', [ARP_BIN, '-d', escapeshellarg($ip)]), $out, $rc);
		if ($rc !== 0) {
			$savemsg = sprintf(gettext('%s could not be deleted.'), $ip);
			$savemsgtype = 'alert-warning';
		}
	} else {
		$savemsg = sprintf(gettext('%s is not a valid IPv4 address.'), $ip);
		$savemsgtype = 'alert-warning';
	}
}

/* clear ARP table */
if (isset($_POST['cleararptable'])) {
	$rc = 0;
	$savemsg = gettext('ARP table has been cleared.');
	$savemsgtype = 'success';

	exec(implode(' ', [ARP_BIN, '-d', '-a']), $out, $rc);
	if ($rc !== 0) {
		$savemsg = gettext('Unable to clear ARP table.');
		$savemsgtype = 'alert-warning';
	}
}

$arp_table = prepare_ARP_table();

$pgtitle = [gettext('Diagnostics'), gettext('ARP Table')];
include('head.inc');

// Handle save msg if defined
if ($savemsg) {
	print_info_box(htmlentities($savemsg), $savemsgtype);
}
?>
<div class="panel panel-default" id="search-panel">
<div class="panel-heading">
	<h2 class="panel-title">
		<?=gettext('Search')?>
		<span class="widget-heading-icon pull-right">
			<a data-toggle="collapse" href="#search-panel_panel-body">
				<i class="fa-solid fa-plus-circle"></i>
			</a>
		</span>
	</h2>
</div>
<div id="search-panel_panel-body" class="panel-body collapse in">
	<div class="form-group">
		<label class="col-sm-2 control-label">
			<?=gettext('Search Term')?>
		</label>
		<div class="col-sm-5"><input class="form-control" name="searchstr" id="searchstr" type="text"/></div>
		<div class="col-sm-2">
			<select id="where" class="form-control">
				<option value="1" selected><?=gettext('All');?></option>
				<option value="2"><?=gettext('Interface')?></option>
				<option value="3"><?=gettext('IP Address')?></option>
				<option value="4"><?=gettext('MAC Address')?></option>
				<option value="5"><?=gettext('Hostname')?></option>
				<option value="6"><?=gettext('Status')?></option>
				<option value="7"><?=gettext('Link Type')?></option>
			</select>
		</div>
		<div class="col-sm-3">
			<a id="btnsearch" title="<?=gettext("Search")?>" class="btn btn-primary btn-sm"><i class="fa-solid fa-search icon-embed-btn"></i><?=gettext("Search")?></a>
			<a id="btnclear" title="<?=gettext("Clear")?>" class="btn btn-info btn-sm"><i class="fa-solid fa-undo icon-embed-btn"></i><?=gettext("Clear")?></a>
		</div>
		<div class="col-sm-10 col-sm-offset-2">
			<span class="help-block"><?=gettext('Enter a search string or *nix regular expression to filter entries.')?></span>
		</div>
	</div>
</div>
</div>

<div class="panel panel-default">
	<div class="panel-heading"><h2 class="panel-title"><?=gettext('ARP Table')?></h2></div>
	<div class="panel-body">
		<div class="table-responsive">
			<table class="sortable-theme-bootstrap table table-striped table-hover" data-sortable>
				<thead>
					<tr>
						<th><?= gettext("Interface")?></th>
						<th><?= gettext("IP Address")?></th>
						<th><?= gettext("MAC Address")?></th>
						<th><?= gettext("Hostname")?></th>
						<th><?= gettext("Status")?></th>
						<th><?= gettext("Link Type")?></th>
						<th style="white-space:nowrap;" data-sortable="false"><?=gettext("Actions")?></th>
					</tr>
				</thead>
				<tbody>

<?php
	foreach ($arp_table as $entry): ?>
				<tr>
					<td><?=$entry['interface']?></td>
					<td><?=$entry['ip-address']?></td>
					<td>
						<?=$entry['mac-address']?>
<?php if ($entry['mac-man']):?>
						<small>(<?=$entry['mac-man']?>)</small>
<?php endif; ?>
					</td>
					<td><?=$entry['dnsresolve']?></td>
					<td><?=$entry['expires']?></td>
					<td><?=$entry['type']?></td>
					<td>
<?php if ($entry['assigned']): /* only useful for assigned interfaces */ ?>
						<a class="fa-solid fa-plus-square" title="<?=gettext('Add WOL mapping')?>" href="services_wol_edit.php?if=<?=htmlspecialchars($entry['if'])?>&amp;mac=<?=htmlspecialchars($entry['mac-address'])?>&amp;descr=<?=htmlspecialchars($entry['dnsresolve'])?>"></a>
						<a class="fa-solid fa-power-off" title="<?=gettext('Send WOL packet')?>" href="services_wol.php?if=<?=htmlspecialchars($entry['if'])?>&amp;mac=<?=htmlspecialchars($entry['mac-address'])?>" usepost></a>
<?php endif; ?>
						<a class="fa-solid fa-trash-can" title="<?=gettext('Delete ARP cache entry')?>" href="diag_arp.php?deleteentry=<?=$entry['ip-address']?>" usepost></a>
					</td>
				</tr>
<?php
	endforeach;
?>
				</tbody>
			</table>
		</div>
	</div>
</div>

<nav class="action-buttons">
	<button id="cleararp" class="btn btn-danger no-confirm">
		<i class="fa-solid fa-trash-can icon-embed-btn"></i>
		<?=gettext("Clear ARP Table")?>
	</button>
</nav>

<script type="text/javascript">
//<![CDATA[
// Clear the "loading" div once the page has loaded"
events.push(function() {
	$('#loading').empty();

	// Make these controls plain buttons
	$("#btnsearch").prop('type', 'button');
	$("#btnclear").prop('type', 'button');

	// Search for a term in the entry name and/or dn
	$("#btnsearch").click(function() {
		var searchstr = $('#searchstr').val().toLowerCase();
		var table = $("table tbody");
		var where = $('#where').val();

		table.find('tr').each(function (i) {
			var $tds = $(this).find('td'),
				iface    = $tds.eq(0).text().trim().toLowerCase(),
				ipaddr   = $tds.eq(1).text().trim().toLowerCase();
				macaddr  = $tds.eq(2).text().trim().toLowerCase();
				hostname = $tds.eq(3).text().trim().toLowerCase();
				stat     = $tds.eq(4).text().trim().toLowerCase();
				linktype = $tds.eq(5).text().trim().toLowerCase();

			regexp = new RegExp(searchstr);
			if (searchstr.length > 0) {
				if (!(regexp.test(iface)    && ((where == 2) || (where == 1))) &&
				    !(regexp.test(ipaddr)   && ((where == 3) || (where == 1))) &&
				    !(regexp.test(macaddr)  && ((where == 4) || (where == 1))) &&
				    !(regexp.test(hostname) && ((where == 5) || (where == 1))) &&
				    !(regexp.test(stat)     && ((where == 6) || (where == 1))) &&
				    !(regexp.test(linktype) && ((where == 7) || (where == 1)))
				    ) {
					$(this).hide();
				} else {
					$(this).show();
				}
			} else {
				$(this).show();	// A blank search string shows all
			}
		});
	});

	// Clear the search term and unhide all rows (that were hidden during a previous search)
	$("#btnclear").click(function() {
		var table = $("table tbody");

		$('#searchstr').val("");

		$('#where option[value="1"]').prop('selected', true);

		table.find('tr').each(function (i) {
			$(this).show();
		});
	});

	// Hitting the enter key will do the same as clicking the search button
	$("#searchstr").on("keyup", function (event) {
		if (event.keyCode == 13) {
			$("#btnsearch").get(0).click();
		}
	});

	$('#cleararp').click(function() {
		if (confirm("<?=gettext('Are you sure you wish to clear ARP table?')?>")) {
			postSubmit({cleararptable: 'true'}, 'diag_arp.php');
		}
	});

});
//]]>
</script>

<div class="infoblock blockopen">
<?php
print_info_box(sprintf(gettext('Local IPv6 peers use %1$sNDP%2$s instead of ARP.'), '<a href="diag_ndp.php">', '</a>') . '<br />' .
   '<br />' . gettext('Permanent ARP entries are shown for local interfaces or static ARP entries.') .
   '<br />' . gettext('Normal dynamic ARP entries show a countdown timer until they will expire and then be re-checked.') .
   '<br />' . gettext('Incomplete ARP entries indicate that the target host has not yet replied to an ARP request.'), 'info', false);
?>
</div>

<?php
include('foot.inc');

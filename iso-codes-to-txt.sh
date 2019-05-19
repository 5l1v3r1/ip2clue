#!/usr/bin/php
<?php

$data = array();
$contents = file_get_contents("/usr/share/xml/iso-codes/iso_3166.xml");
if (!$contents)
	die("Canot open file. Do you have iso-codes package installed?");

$parser = xml_parser_create('');
if(!$parser)
	die("Cannot create parser!");

xml_parser_set_option($parser, XML_OPTION_TARGET_ENCODING, 'UTF-8');
xml_parser_set_option($parser, XML_OPTION_CASE_FOLDING, 0);
xml_parser_set_option($parser, XML_OPTION_SKIP_WHITE, 1);
xml_parse_into_struct($parser, trim($contents), $data);
xml_parser_free($parser);
if (!$data)
	die("Cannot load data from xml!");

$out = "";
foreach ($data as $junk => $info) {
	if (strcmp($info['tag'], "iso_3166_entry") != 0)
		continue;
	if (strcmp($info['type'], "complete") != 0)
		continue;

	$a = $info['attributes'];

	$out .= $a['alpha_2_code']
		. "," . $a['alpha_3_code']
		. "," . $a['name']
		. "\n";
}

file_put_contents("data/iso_3166.csv", $out);
?>

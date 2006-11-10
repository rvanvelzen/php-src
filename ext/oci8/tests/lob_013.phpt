--TEST--
lob buffering
--SKIPIF--
<?php if (!extension_loaded('oci8')) die("skip no oci8 extension"); ?>
--FILE--
<?php
	
require dirname(__FILE__).'/connect.inc';
require dirname(__FILE__).'/create_table.inc';

$ora_sql = "INSERT INTO
                       ".$schema.$table_name." (blob)
                      VALUES (empty_blob())
                      RETURNING
                               blob
                      INTO :v_blob ";

$statement = oci_parse($c,$ora_sql);
$blob = oci_new_descriptor($c,OCI_D_LOB);
oci_bind_by_name($statement,":v_blob", $blob,-1,OCI_B_BLOB);
oci_execute($statement, OCI_DEFAULT);

var_dump($blob->write(b"test"));
var_dump($blob->getBuffering());
var_dump($blob->setBuffering(true));
var_dump($blob->write(b"test"));
var_dump($blob->getBuffering());
var_dump($blob->flush());

oci_commit($c);

$select_sql = "SELECT blob FROM ".$schema.$table_name."";
$s = oci_parse($c, $select_sql);
oci_execute($s);

$row = oci_fetch_array($s, OCI_RETURN_LOBS);

var_dump($row[0]);


require dirname(__FILE__).'/drop_table.inc';

echo "Done\n";

?>
--EXPECT--
int(4)
bool(false)
bool(true)
int(4)
bool(true)
bool(true)
string(8) "testtest"
Done

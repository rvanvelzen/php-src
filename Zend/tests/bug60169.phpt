--TEST--
Bug #60169 (Conjunction of ternary and list crashes PHP)
--XFAIL--
See Bug #60169, doesn't fixed yet
--FILE--
<?php
error_reporting(0);
$arr  = array("test");
list($a,$b)= is_array($arr)? $arr : $arr;
echo "ok\n";
--EXPECT--
ok

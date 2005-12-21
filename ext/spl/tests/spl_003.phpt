--TEST--
SPL: class_parents() and class_implements()
--SKIPIF--
<?php if (!extension_loaded("spl")) print "skip"; ?>
--SKIPIF--
<?php if (!extension_loaded("spl")) print "skip"; ?>
--FILE--
<?php
class a{}
class b extends a{}
class c extends b{}
class d{}
var_dump(class_parents(new c),
         class_parents("c"),
         class_parents(new b),
         class_parents("b"),
         class_parents("d"),
         class_parents("foo", 0),
         class_parents("foo", 1)
);

interface iface1{}
interface iface2{}
class f implements iface1, iface2{}
var_dump(class_implements(new a),
         class_implements("a"),
         class_implements("aaa"),
         class_implements("bbb", 0)
);

function __autoload($cname) {
    var_dump($cname);
}

?>
===DONE===
<?php exit(0); ?>
--EXPECTF--
Warning: class_parents(): Class foo does not exist in %sspl_003.php on line %d
string(3) "foo"

Warning: class_parents(): Class foo does not exist and could not be loaded in %sspl_003.php on line %d
array(2) {
  ["b"]=>
  string(1) "b"
  ["a"]=>
  string(1) "a"
}
array(2) {
  ["b"]=>
  string(1) "b"
  ["a"]=>
  string(1) "a"
}
array(1) {
  ["a"]=>
  string(1) "a"
}
array(1) {
  ["a"]=>
  string(1) "a"
}
array(0) {
}
bool(false)
bool(false)
string(3) "aaa"

Warning: class_implements(): Class aaa does not exist and could not be loaded in %sspl_003.php on line %d

Warning: class_implements(): Class bbb does not exist in %sspl_003.php on line %d
array(0) {
}
array(0) {
}
bool(false)
bool(false)
===DONE===
--UEXPECTF--
Warning: class_parents(): Class foo does not exist in %sspl_003.php on line %d
unicode(3) "foo"

Warning: class_parents(): Class foo does not exist and could not be loaded in %sspl_003.php on line %d
array(2) {
  [u"b"]=>
  unicode(1) "b"
  [u"a"]=>
  unicode(1) "a"
}
array(2) {
  [u"b"]=>
  unicode(1) "b"
  [u"a"]=>
  unicode(1) "a"
}
array(1) {
  [u"a"]=>
  unicode(1) "a"
}
array(1) {
  [u"a"]=>
  unicode(1) "a"
}
array(0) {
}
bool(false)
bool(false)
unicode(3) "aaa"

Warning: class_implements(): Class aaa does not exist and could not be loaded in %sspl_003.php on line %d

Warning: class_implements(): Class bbb does not exist in %sspl_003.php on line %d
array(0) {
}
array(0) {
}
bool(false)
bool(false)
===DONE===
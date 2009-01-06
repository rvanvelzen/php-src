--TEST--
Persistent connections and mysqli.max_links
--SKIPIF--
<?php
	require_once('skipif.inc');
	require_once('skipifemb.inc');
	require_once('skipifconnectfailure.inc');
	require_once('connect.inc');

	if (!$IS_MYSQLND)
		die("skip mysqlnd only test");

	// we need a second DB user to test for a possible flaw in the ext/mysql[i] code
	if (!$link = mysqli_connect($host, $user, $passwd, $db, $port, $socket))
		die(sprintf("skip Cannot connect [%d] %s", mysqli_connect_errno(), mysqli_connect_error()));

	mysqli_query($link, 'DROP USER pcontest');
	if (!mysqli_query($link, 'CREATE USER pcontest IDENTIFIED BY "pcontest"')) {
		printf("skip Cannot create second DB user [%d] %s", mysqli_errno($link), mysqli_error($link));
		mysqli_close($link);
		die();
	}

	// we might be able to specify the host using CURRENT_USER(), but...
	if (!mysqli_query($link, sprintf("GRANT SELECT ON TABLE %s.test TO pcontest@'%%'", $db))) {
		printf("skip Cannot GRANT SELECT to second DB user [%d] %s", mysqli_errno($link), mysqli_error($link));
		mysqli_query($link, 'REVOKE ALL PRIVILEGES, GRANT OPTION FROM pcontest');
		mysqli_query($link, 'DROP USER pcontest');
		mysqli_close($link);
		die();
	}
	mysqli_close($link);
?>
--INI--
mysqli.allow_persistent=1
mysqli.max_persistent=2
--FILE--
<?php
	require_once("connect.inc");
	require_once('table.inc');

	if (!$plink = mysqli_connect('p:' . $host, 'pcontest', 'pcontest', $db, $port, $socket))
		printf("[001] Cannot connect using the second DB user created during SKIPIF, [%d] %s\n",
			mysqli_connect_errno(), mysqli_connect_error());

	ob_start();
	phpinfo();
	$phpinfo = strip_tags(ob_get_contents());
	ob_end_clean();

	$phpinfo = substr($phpinfo, strpos($phpinfo, 'MysqlI Support => enabled'), 500);
	if (!preg_match('@Active Persistent Links\s+=>\s+(\d+)@ismU', $phpinfo, $matches))
		printf("[002] Cannot get # active persistent links from phpinfo()\n");
	$num_plinks = $matches[1];

	if (!$res = mysqli_query($plink, 'SELECT id, label FROM test WHERE id = 1'))
		printf("[003] Cannot run query on persistent connection of second DB user, [%d] %s\n",
			mysqli_errno($plink), mysqli_error($plink));

	if (!$row = mysqli_fetch_assoc($res))
		printf("[004] Cannot run fetch result, [%d] %s\n",
			mysqli_errno($plink), mysqli_error($plink));
	mysqli_free_result($res);
	var_dump($row);

	// change the password for the second DB user and kill the persistent connection
	if (!mysqli_query($link, 'SET PASSWORD FOR pcontest = PASSWORD("newpass")') ||
			!mysqli_query($link, 'FLUSH PRIVILEGES'))
		printf("[005] Cannot change PW of second DB user, [%d] %s\n", mysqli_errno($link), mysqli_error($link));

	// persistent connections cannot be closed but only be killed
	$pthread_id = mysqli_thread_id($plink);
	if (!mysqli_query($link, sprintf('KILL %d', $pthread_id)))
		printf("[006] Cannot KILL persistent connection of second DB user, [%d] %s\n", mysqli_errno($link), mysqli_error($link));
	// give the server a second to really kill the thread
	sleep(1);

	if (!$res = mysqli_query($link, "SHOW FULL PROCESSLIST"))
		printf("[007] [%d] %s\n", mysqli_errno($link), mysqli_error($link));

	$running_threads = array();
	while ($row = mysqli_fetch_assoc($res))
		$running_threads[$row['Id']] = $row;
	mysqli_free_result($res);

	if (isset($running_threads[$pthread_id]))
		printf("[008] Persistent connection has not been killed\n");

	// this fails and we have 0 (<= $num_plinks) connections
	if ($plink = @mysqli_connect('p:' . $host, 'pcontest', 'pcontest', $db, $port, $socket))
		printf("[009] Can connect using the old password, [%d] %s\n",
			mysqli_connect_errno($link), mysqli_connect_error($link));

	ob_start();
	phpinfo();
	$phpinfo = strip_tags(ob_get_contents());
	ob_end_clean();
	$phpinfo = substr($phpinfo, stripos($phpinfo, 'MysqlI Support => enabled'), 500);
	if (!preg_match('@Active Persistent Links\s+=>\s+(\d+)@ismU', $phpinfo, $matches))
		printf("[010] Cannot get # of active persistent links from phpinfo()\n");

	$num_plinks_kill = $matches[1];
	if ($num_plinks_kill > $num_plinks)
		printf("[011] Expecting Active Persistent Links < %d, got %d\n", $num_plinks, $num_plinks_kill);

	if (!$plink = mysqli_connect('p:' . $host, 'pcontest', 'newpass', $db, $port, $socket))
		printf("[012] Cannot connect using the new password, [%d] %s\n",
			mysqli_connect_errno(), mysqli_connect_error());

	if (!$res = mysqli_query($plink, 'SELECT id, label FROM test WHERE id = 1'))
		printf("[013] Cannot run query on persistent connection of second DB user, [%d] %s\n",
			mysqli_errno($plink), mysqli_error($plink));

	if (!$row = mysqli_fetch_assoc($res))
		printf("[014] Cannot run fetch result, [%d] %s\n",
			mysqli_errno($plink), mysqli_error($plink));
	mysqli_free_result($res);
	var_dump($row);

	if ($plink2 = mysqli_connect('p:' . $host, 'pcontest', 'newpass', $db, $port, $socket))
		printf("[015] Can open more persistent connections than allowed, [%d] %s\n",
			mysqli_connect_errno(), mysqli_connect_error());

	ob_start();
	phpinfo();
	$phpinfo = strip_tags(ob_get_contents());
	ob_end_clean();
	$phpinfo = substr($phpinfo, stripos($phpinfo, 'MysqlI Support => enabled'), 500);
	if (!preg_match('@Active Persistent Links\s+=>\s+(\d+)@ismU', $phpinfo, $matches))
		printf("[016] Cannot get # of active persistent links from phpinfo()\n");

	$num_plinks = $matches[1];
	if ($num_plinks > (int)ini_get('mysqli.max_persistent'))
		printf("[017] mysqli.max_persistent=%d allows %d open connections!\n", ini_get('mysqli.max_persistent'),$num_plinks);

	mysqli_query($link, 'REVOKE ALL PRIVILEGES, GRANT OPTION FROM pcontest');
	mysqli_query($link, 'DROP USER pcontest');
	mysqli_close($link);
	print "done!";
?>
--EXPECTF--
array(2) {
  [u"id"]=>
  unicode(1) "1"
  [u"label"]=>
  unicode(1) "a"
}
array(2) {
  [u"id"]=>
  unicode(1) "1"
  [u"label"]=>
  unicode(1) "a"
}

Warning: mysqli_connect(): Too many open persistent links (%d) in %s on line %d
done!

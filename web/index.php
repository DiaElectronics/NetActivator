<html>
<head>
<title>POST 2</title>
</head>
<body>
<p>
POST #2
</p>
<p>
Last action:
<?php
if($_POST["actid"]) {
	$sock = socket_create(AF_INET, SOCK_DGRAM, SOL_UDP);
	$msg = "1:+8/900000;";
	if($_POST["actid"]=="water") {
		echo "water engaged";
		$msg = "1:+1/60000|+8/60000|+7/60000|-2|-3|-4|-5|-6;";
	} else {
		echo "light engaged";
	}

	$len = strlen($msg);

	socket_sendto($sock, $msg, $len, 0, '192.168.14.100', 6001);
	socket_close($sock);
}

?>
</p>

<p>
<form action="" method="POST">
<input type="hidden" id="actid" name="actid" value="water" />
<input type="submit" value="WATER(60 sec)"></input>
</form>
</p>
<p>
<form action="" method="POST">
<input type="hidden" id="actid" name="actid" value="light" />
<input type="submit" value="LIGHT(90 sec)"></input>
</form>
</p>
</body>
</html>

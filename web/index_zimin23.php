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
	$msg = "1:+8/1200000;";
	if($_POST["actid"]=="water") {
		echo "water engaged";
		$msg = "1:+1/120000|+8/120000|+7/120000|-2|-3|-4|-5|-6;";
	} elseif($_POST["actid"]=="light")
	{
		$msg = "1:+8/3000000;";
		echo "light engaged";
	} elseif($_POST["actid"]=="soap")
	{
		$msg = "1:+1/120000|+8/120000|+7/120000|+3/120000/135/350|-2|-4|-5|-6";
		echo "Soap engaged";
	}
	elseif($_POST["actid"]=="soappremium")
	{
		$msg = "1:+1/120000|+8/120000|+7/120000|+3/120000/250/250|-2|-4|-5|-6";
		echo "Soap Premium engaged";
	} elseif($_POST["actid"]=="wax")
	{
		$msg = "1:+1/118000|+8/118000|+7/118000|+5/118000/75/425|-2|-3|-4|-6;";
		echo "Wax engaged";
	}
	elseif($_POST["actid"]=="stop")
	{
		$msg = "1:-1|-2|-3|-4|-5|-6|-7|-8;";
		echo "STOP";
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
<input type="submit" value="WATER(120 sec)"></input>
</form>
</p>
<p>
<form action="" method="POST">
<input type="hidden" id="actid" name="actid" value="soap" />
<input type="submit" value="SOAP (120 sec)"></input>
</form>
</p>
<p>
<form action="" method="POST">
<input type="hidden" id="actid" name="actid" value="soappremium" />
<input type="submit" value="SOAP(PREMIUM)(120 sec)"></input>
</form>
</p>
<p>
<form action="" method="POST">
<input type="hidden" id="actid" name="actid" value="wax" />
<input type="submit" value="WAX(118 sec)"></input>
</form>
</p>
<p>
<form action="" method="POST">
<input type="hidden" id="actid" name="actid" value="light" />
<input type="submit" value="LIGHT(300 sec)"></input>
</form>
</p>
<p>
<form action="" method="POST">
<input type="hidden" id="actid" name="actid" value="stop" />
<input type="submit" value="STOP"></input>
</form>
</p>
</body>
</html>

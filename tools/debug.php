<?php
define('TYPE',1);
define('PROC',2);
define('ADDRESS',3);
define('SIZE',4);
define('CALLERNAME',5);

if($argc < 2) {
	printf("Usage: %s <file>\n",$argv[0]);
	return 1;
}

$allocs = array();
$matches = array();
$content = implode('',file($argv[1]));
preg_match_all('/\[(A|F)\] p=([^ ]+) a=([\da-f]+) s=(\d+) c=([a-z0-9A-Z_]+)/',$content,$matches);

foreach($matches[0] as $k => $v) {
	$addr = $matches[ADDRESS][$k];
	if(!isset($allocs[$addr]))
		$allocs[$addr] = array(0,0,0,'');
	if($matches[TYPE][$k] == 'A') {
		$allocs[$addr][0]++;
		$allocs[$addr][1] = $matches[SIZE][$k];
		$allocs[$addr][2] = $matches[PROC][$k];
		$allocs[$addr][3] = $matches[CALLERNAME][$k];
	}
	else
		$allocs[$addr][0]--;
}

foreach($allocs as $addr => $open) {
	if($open[0] > 0)
		echo $addr.' => '.$open[0].' (size='.$open[1].', proc='.$open[2].', caller='.$open[3].')'."\n";
}
?>
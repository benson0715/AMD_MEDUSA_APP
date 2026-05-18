use strict;
use Cwd;
use Getopt::Long;
use Data::Dumper;
use File::Copy;
use File::Path;
use File::stat;

my %colName2Idx = (
#   Column Name      Column Index
	DEVICE_ADDR        => 'A',
	COMMAND            => 'B',
	REG_NAME           => 'D',   # Register name of Page 0
	
	PAGE_0_VAL         => 'F',
	PAGE_0_WIDTH       => 'E',

	PAGE_1_VAL         => 'I',
	PAGE_1_WIDTH       => 'H',

	PAGE_2_VAL         => 'L',
	PAGE_2_WIDTH       => 'K',

	PAGE_3_VAL         => 'O',
	PAGE_3_WIDTH       => 'N',

	PAGE_4_VAL         => 'R',
	PAGE_4_WIDTH       => 'Q',
);

my %cmbTab = ();

my @tmpArray = ();
my @desp = ();

sub _IdxChar2Num()
{
	my $idxChar = shift;
	
	if ($idxChar =~ /^[A-Z]+$/) {
		if (length($idxChar) == 1) {
			return ord($idxChar) - ord('A');
		} elsif (length($idxChar) == 2) {
			return 26 + ord(substr($idxChar, 1, 1)) - ord('A');
		}
	}
	
	die 'Unknown column index ' + $idxChar;
}

sub _getColumn()
{
	my $IdxChar = shift;
	my $idx = &_IdxChar2Num($colName2Idx{$IdxChar});
	return "" if ($idx > $#tmpArray);
	if ($tmpArray[$idx] =~ /__DESP_PLACE_HOLDER_(\d+)_/) {
		my $t = $1;
		return "" if ($t > $#desp);
		return $desp[$t];
	}
	return $tmpArray[$idx];
}

&main();

my @dirFilesArr;
sub listFilesOfDir
{
	my $root = shift;
	
	$root =~ s/[\\\/]$//;
	@dirFilesArr = ();
	&_fetchSubDir($root);
	
	return \@dirFilesArr;
}

sub _fetchSubDir
{
	my $curRoot = shift;
	my @thisLevel = ();
	if ( opendir DIR, $curRoot ) {
		@thisLevel = readdir DIR;
		closedir DIR;
	} else {
		print ("Cannot access $curRoot. Skipped...\n");
	}
	
	foreach my $obj (@thisLevel) {
		if ($obj ne "." && $obj ne ".." && -d $curRoot."/".$obj) {
			&_fetchSubDir($curRoot."/".$obj);
		} elsif ($obj ne "." && $obj ne "..") {
			push @dirFilesArr, $curRoot."/".$obj;
		}
	}
}

sub readFile {
	my $theFile = shift;
	my @lines = ();
	
	my $file_opened = open( my $FILE, "<", $theFile );
	unless ($file_opened) {
		print "Unable to open $theFile\n";
		return \@lines;
	}
	
	@lines = <$FILE>;
	close($FILE);
	return \@lines;
}

sub findFilesByPtn
{
	my $baseDir = shift;
	my $ptn = shift;
	my @allMatches = grep /$ptn/i, @{ listFilesOfDir($baseDir) };
	return \@allMatches;
}


sub _splitCsvLine()
{
	my $line = shift;
	
	@tmpArray = ();
	@desp = ();
	my $i = 0;
	while ($line =~ /"([^"]+)"/) {
		my $tmp = $1;
		push @desp, $tmp;
		$tmp = quotemeta($tmp);
		$line =~ s/"$tmp"/__DESP_PLACE_HOLDER_${i}_/;
		$i++;
	}
	@tmpArray = split /\s*,\s*/, $line;
}

sub insertCsv2Hash
{
	my $pTab = shift;
	my $reg = &_getColumn('COMMAND');
	
	die "Duplicated define of $reg from CfgTable.\n" if (exists $pTab->{$reg});
	
	foreach my $key (keys %colName2Idx) {
		$pTab->{$reg}->{$key} = &_getColumn($key);
	}
}

sub loadCsvTable
{
	my $filename = shift;
	my @csvFile = @{ &readFile($filename) };
	
	my %vrcfg = ();
	
	my $step = 1;
	for (my $i = 0; $i <= $#csvFile; $i += $step) {
		my $line = $csvFile[$i];
		chomp $line;
	
		$step = 1;
		if ($line !~ /^0x[a-fA-F0-9]+,/) {
			print "Skip ... $line\n";
			next;
		}

		&_splitCsvLine($line);
		insertCsv2Hash(\%vrcfg);
	}
	
	return \%vrcfg;
}

my $INDIR;
my $OUTDIR;
my $FILEPTN;

sub _Int2bString ($$)
{
	my $u32Setting = shift;
	my $bits = shift;
	my $bSetting = "";

	for (my $i = 0; $i < $bits; $i ++) {
		if ($i % 4 == 0 && $i != 0) {
			$bSetting = "," . $bSetting;
		}
		$bSetting = (($u32Setting & (0x01 << $i)) ? "1" : "0") . $bSetting;
	}
	
	return $bSetting;
}

sub checkValid($$$$)
{
	#    str   int     int    str
	my ($reg, $table, $page, $valStr) = @_;
	my $ret = 0;
		
	for(my $t = 0; $t < 30; $t++) {
		for(my $p = 0; $p < 10; $p++) {
			if (((1 << $t) & $table) && ((1 << $p) & $page)) {
				my $key = sprintf "t%d_p%d", $t, $p;
				if (exists $cmbTab{$reg}->{$key} && $cmbTab{$reg}->{$key} ne '') {
					if ($cmbTab{$reg}->{$key} ne $valStr) {
						return 0;  # conflict, to add a new one
					} else {
						$ret = 2;  # no violate, or togather
					}					
				}
			}
		}
	}
	
	return $ret if ($ret);         # existing, skip it
	return 1;                      # not existing
}

sub deleteData($$$$)
{
	#    str   int     int    str
	my ($reg, $table, $page, $valStr) = @_;
	my $ret = 0;
		
	for(my $t = 0; $t < 30; $t++) {
		for(my $p = 0; $p < 10; $p++) {
			if (((1 << $t) & $table) && ((1 << $p) & $page)) {
				my $key = sprintf "t%d_p%d", $t, $p;
				if (exists $cmbTab{$reg}->{$key}) {
					if ($cmbTab{$reg}->{$key} eq $valStr) {
						$cmbTab{$reg}->{$key} = '';
					}					
				}
			}
		}
	}
}

sub main
{
	# get commandline options
	&GetOptions (	'indir=s' => \$INDIR,
					'outdir=s' => \$OUTDIR,
					'inputPtn=s' => \$FILEPTN
				) or die $!;

	die "Incorrect folder path $OUTDIR.\n" if (not -d $OUTDIR);
	die "Incorrect folder path $INDIR.\n"  if (not -d $INDIR);
	
	my @tables = ();
	my $macros = "";

	my @cfgTablesFile = @ { &findFilesByPtn($INDIR, "$FILEPTN\$") };
	# Sort files by filename to ensure consistent ordering
	@cfgTablesFile = sort { 
		# Extract filename from full path for comparison
		my ($aFile) = $a =~ /([^\\\/]+)$/;
		my ($bFile) = $b =~ /([^\\\/]+)$/;
		$aFile cmp $bFile;
	} @cfgTablesFile;
	my $tabIdx = 0;
	foreach my $table (@cfgTablesFile) {
		print "load $table ...\n";
		
		push @tables, &loadCsvTable($table);
		
		$table =~ /([^\\\/]+)$/;
		my $tableName = $1;
		my $tableVer = "";
		# if (exists $tables[$#tables]->{'0x85'} && $tables[$#tables]->{'0x85'}->{'REG_NAME'} =~ /\s*PRODUCT_DATA_CODE\s*/i) {    # 0x85 is used as PRODUCT_DATA_CODE （Page0 is CODE_VER）
		# 	$tables[$#tables]->{'0x85'}->{'PAGE_1_VAL'} =~ /(0x)?([^\s]+)/;
		# 	$tableVer = $2;
		# } else {
		# 	die "Table $tableName doesn't has CODE_REV ...\n";
		# }
		
		$tableVer = "v".$tableVer;
		$macros .= sprintf "#define VR_CFG_TABLE_$tabIdx     ($tabIdx)      // %-6s $tableName\n", $tableVer;

		$tabIdx ++;
	}
	
	$tabIdx = 0;
	foreach my $pTable (@tables) {
		print "Processing table $tabIdx ...\n";
		
		foreach my $reg (sort {$a cmp $b} keys %{ $pTable }) {
			if (not exists $cmbTab{$reg}) {
				my %tmp = ();
				$cmbTab{$reg} = \%tmp;
				$cmbTab{$reg}->{'REG_NAME'} = $pTable->{$reg}->{'REG_NAME'};
			}
			
			# try at most 7 pages
			for my $p (0 .. 7) {
				my $key = sprintf "PAGE_%d_VAL", $p;
				if (exists $pTable->{$reg}->{$key}) {
					$cmbTab{$reg}->{"t${tabIdx}_p${p}"} = $pTable->{$reg}->{$key};
				}
				
				if (not exists $cmbTab{$reg}->{'WIDTH'}) {
					$cmbTab{$reg}->{'WIDTH'} = $pTable->{$reg}->{'PAGE_0_WIDTH'};
				}

				$key = sprintf "PAGE_%d_WIDTH", $p;
				# if (exists $pTable->{$reg}->{$key} && $pTable->{$reg}->{$key} ne '') {
				# 	die "Width is not match for $reg" if ($pTable->{$reg}->{$key} != $cmbTab{$reg}->{'WIDTH'});
				# }
			}
		}

		$tabIdx ++;
	}
	
	print "Sort the data ...\n";
	foreach my $reg (sort {$a cmp $b} keys %cmbTab) {
		# handle $cmbTab{$reg}
		
		my %inverted = ();
		foreach my $key (sort {$a cmp $b} keys %{ $cmbTab{$reg} } ) {
			if ($key =~ /t(\d+)_p(\d+)/) {
				my $table = $1;
				my $page  = $2;
				my $valStr = $cmbTab{$reg}->{$key};
				my $value = 0xFFFF;
				
				chomp $valStr;

				if ($valStr =~ /^\s*0x([0-9A-Fa-f]+)\s*$/) {
					$value = hex $1;
				} elsif ($valStr !~ /^\s*$/) {
					die "Incorrect value $valStr of $reg in table $table page $page";
				}

				if ($value != 0xFFFF) {
					if (not exists $inverted{$valStr}) {
						#         reg,  table, page, value 
						my @tmp = ($reg, (1 << $table), (1 << $page), $valStr);
						$inverted{$valStr} = \@tmp;
					} else {
						my $pArr = $inverted{$valStr};
						my $flag = 0;
						
						for (my $i = 0; $i <= $#$pArr ; $i += 4) {
							my $tmpTab = $pArr->[$i + 1] | (1 << $table);
							my $tmpPag = $pArr->[$i + 2] | (1 << $page);
							
							my $st = &checkValid($reg, $tmpTab, $tmpPag, $valStr);
							if ($st != 0) {  # if no violate if OR-ed togather
								$pArr->[$i + 1] = $tmpTab;   # bitmap for Table
								$pArr->[$i + 2] = $tmpPag;   # bitmap for Page
								$flag = 1;
								last;
							}
						}
						
						if (!$flag) {
							push @{$pArr}, $reg, (1 << $table), (1 << $page), $valStr;  # create new one
						}
					}
				}
			}
		}
		
		$cmbTab{$reg}->{'INVERTED'} = \%inverted;
	}
	
	print "Check the data ...\n";
	foreach my $reg (sort {$a cmp $b} keys %cmbTab) {
		foreach my $val (sort { 
			# Sort by table first, then by page
			$cmbTab{$reg}->{'INVERTED'}->{$a}->[1] <=> $cmbTab{$reg}->{'INVERTED'}->{$b}->[1] ||
			$cmbTab{$reg}->{'INVERTED'}->{$a}->[2] <=> $cmbTab{$reg}->{'INVERTED'}->{$b}->[2] 
		} keys %{ $cmbTab{$reg}->{'INVERTED'} }) {
			my $pArr = $cmbTab{$reg}->{'INVERTED'}->{$val};
			for (my $i = 0; $i <= $#$pArr ; $i += 4) {
				my $tableMask = $pArr->[$i + 1];
				my $pageMask  = $pArr->[$i + 2];
				
				if (2 != &checkValid($reg, $tableMask, $pageMask, $val)) {
					die "Error on check reg $reg value $val";
				}
				
				&deleteData($reg, $tableMask, $pageMask, $val);
			}
		}
	}
	foreach my $reg (sort {$a cmp $b} keys %cmbTab) {
		for(my $t = 0; $t < 30; $t++) {
			for(my $p = 0; $p < 10; $p++) {
				my $key = sprintf "t%d_p%d", $t, $p;
				if (exists $cmbTab{$reg}->{$key} && $cmbTab{$reg}->{$key} ne '') {
					die "Incorrect data when handle $reg $key $cmbTab{$reg}->{$key}";					
				}
			}
		}
	}
	
	print "Generating code ...\n";
	my $dataDefine = "";
	my $lineNum = 0;
	my $lastRegName = "";
	foreach my $reg (sort {$a cmp $b} keys %cmbTab) {
		foreach my $val (sort { 
			# Sort by table first, then by page
			$cmbTab{$reg}->{'INVERTED'}->{$a}->[1] <=> $cmbTab{$reg}->{'INVERTED'}->{$b}->[1] ||
			$cmbTab{$reg}->{'INVERTED'}->{$a}->[2] <=> $cmbTab{$reg}->{'INVERTED'}->{$b}->[2] 
		} keys % { $cmbTab{$reg}->{'INVERTED'} }) {
			my $pArr = $cmbTab{$reg}->{'INVERTED'}->{$val};
			for (my $i = 0; $i <= $#$pArr ; $i += 4) {
				$dataDefine .= sprintf "  { .reg = $reg, .table = 0x%03X, .page = 0x%X, .val = %7s },    // ", $pArr->[$i + 1], $pArr->[$i + 2], $val;
				$dataDefine .= sprintf "T %s, P %s", &_Int2bString($pArr->[$i + 1], 16), &_Int2bString($pArr->[$i + 2], 5), $cmbTab{$reg}->{REG_NAME};
			
				if ($lastRegName ne $cmbTab{$reg}->{REG_NAME}) {
					$lastRegName = $cmbTab{$reg}->{REG_NAME};
					$dataDefine .= sprintf ", %s\n", $lastRegName;
				} else {
						$dataDefine .= ", ...\n";
				}

				$lineNum ++;
			}
		}
	}
	
	$dataDefine = "DEV_SVI3_CPMPRESSED g_svi3VrCfg[] = {\n\n".$dataDefine;
	$dataDefine .= "\n  { .reg = 0xFF, .table = 0x000, .page = 0x0, .val = 0xFFFF }\n};";
	
	open SRC_FILE, ">$OUTDIR/g_vrCfgTable.txt";
	print SRC_FILE $dataDefine."\n";
	print SRC_FILE "\n";
	print SRC_FILE $macros."\n";
	close SRC_FILE;
	
	printf "Done ... \n\nSizeof VrCfg Table %d bytes, print to $OUTDIR/g_vrCfgTable.txt\n\n", $lineNum * 5;
}

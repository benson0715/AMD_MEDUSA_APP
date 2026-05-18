#!/usr/bin/perl
#******************************************************************************
# Copyright (C) 2023 Advanced Micro Devices, Inc. All rights reserved.
#******************************************************************************

use strict;
use Getopt::Long;
use Data::Dumper;
use IO::File;
use File::Basename;

# CSV file defination
my %colName2Idx_IOEXP = (
#   Column Name      Column Index
	CHIP_ID          => 'A',
	SLAVE_ADDR       => 'B',
	I2C_PORT         => 'C',
	I2C_SPEED        => 'D',
	PIN_INDEX        => 'E',
	NET_NAME         => 'F',
	BUF_TYPE         => 'G',
	POR_SETTING      => 'H',
	CATEGORY         => 'I',
	INT_EN           => 'J',
	INT_HANDLER      => 'K',
	DESCRIPTION      => 'L',
);

use constant {
	Bit0             => 0x00000001,
	Bit1             => 0x00000002, 
	Bit2             => 0x00000004, 
	Bit3             => 0x00000008, 
	Bit4             => 0x00000010, 
	Bit5             => 0x00000020, 
	Bit6             => 0x00000040, 
	Bit7             => 0x00000080, 
	Bit8             => 0x00000100, 
	Bit9             => 0x00000200, 
	Bit10            => 0x00000400, 
	Bit11            => 0x00000800, 
	Bit12            => 0x00001000, 
	Bit13            => 0x00002000, 
	Bit14            => 0x00004000, 
	Bit15            => 0x00008000, 
	Bit16            => 0x00010000, 
	Bit17            => 0x00020000, 
	Bit18            => 0x00040000, 
	Bit19            => 0x00080000, 
	Bit20            => 0x00100000, 
	Bit21            => 0x00200000, 
	Bit22            => 0x00400000, 
	Bit23            => 0x00800000, 
	Bit24            => 0x01000000, 
	Bit25            => 0x02000000, 
	Bit26            => 0x04000000, 
	Bit27            => 0x08000000, 
	Bit28            => 0x10000000, 
	Bit29            => 0x20000000, 
	Bit30            => 0x40000000, 
	Bit31            => 0x80000000, 
};

my $OUTDIR;
# my $INPUT;    # the main table
my $IOEXP;    # the table for IO expander
my $REFHFILE;
my $PROGRAM;

my %GPIO_REG = ();
my %PIN_DEF = ();
my %IO_EXP_PIN_DEF = ();
my %IO_EXP_INIT_REG = ();

# collection for BGPO and GPIO
my %BGPO_TAB = ();

my $headerFile = "";
my $gpioAutoGen = "";
my $gpioCount = 0;
my %EC_ACPI_GROUP = ();
my @PorInitCategorys = ();
my $ioExpCount = 0;

my $AcpiGpioHandlerDeclare;

&main();

#######################
sub main ()
{
	&initCmdEnv();
	&loadIoExpTable();
	&calcPinDefinition();
	
	my $fh;
	open $fh, ">".$OUTDIR.$headerFile or die "Cannot open $OUTDIR$headerFile for write.\n";
	&genCopyright($fh, $headerFile, "Project special defination for $PROGRAM");
	&genHeader($fh);
	&autoGenHeaderFile($fh);
	&genTail($fh);
	close $fh;

	my @bufH = ();
	open $fh, "<".$OUTDIR.$headerFile or die "Cannot open $OUTDIR$headerFile for read.\n";
	@bufH = <$fh>;
	close $fh;

	print "\n !!!DONE!!!\n\n";
	printf "# %4d: $OUTDIR.$headerFile\n", $#bufH;
	print "\n\n";
}

#######################
sub initCmdEnv
{
	# get commandline options
	&GetOptions (	'outdir=s' => \$OUTDIR,
					'ioexpcsv=s' => \$IOEXP,
					'program=s' => \$PROGRAM,
				) or die $!;

	$OUTDIR =~ s/[\\\/]\s*$//;
	$OUTDIR =~ s/[\\\/]/\//;
	$OUTDIR .= "/";
	die "$OUTDIR is unavailable.\n" if (not -d $OUTDIR);
	
	die "Undefined program name.\n" if (not defined $PROGRAM);
	$headerFile = "board_ioexpTable.h";
	$gpioAutoGen = "gpioAutoGen.c";
	unlink $headerFile if (-e $OUTDIR.$headerFile);
	unlink $gpioAutoGen if (-e $OUTDIR.$gpioAutoGen);
}

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

my @tmpArray = ();
my @desp = ();
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

sub _getColumnIoExp()
{
	my $IdxChar = shift;
	my $idx = &_IdxChar2Num($colName2Idx_IOEXP{$IdxChar});
	return "" if ($idx > $#tmpArray);
	if ($tmpArray[$idx] =~ /__DESP_PLACE_HOLDER_(\d+)_/) {
		my $t = $1;
		return "" if ($t > $#desp);
		return $desp[$t];
	}
	return $tmpArray[$idx];
}

sub insertIoExpCsv2Hash
{
	my $pinId = &_getColumnIoExp('I2C_PORT') . "_" . &_getColumnIoExp('SLAVE_ADDR') . "_" . &_getColumnIoExp('PIN_INDEX');
	
	die "Duplicated define of $pinId from $IOEXP.\n" if (exists $IO_EXP_PIN_DEF{$pinId});
	
	foreach my $key (keys %colName2Idx_IOEXP) {
		$IO_EXP_PIN_DEF{$pinId}->{$key} = &_getColumnIoExp($key);
	}

	# override NET NAME
	my $netName = $IO_EXP_PIN_DEF{$pinId}->{NET_NAME};
	$netName =~ s/#/n/g;
	$netName =~ s/[^A-Za-z0-9_]/_/g;
	$netName =~ s/_*$//g;
	
	if ($netName !~ /^\s*$/) {
		$IO_EXP_PIN_DEF{$pinId}->{'srcNET_NAME'} = "ex_".$netName;
		$IO_EXP_PIN_DEF{$pinId}->{'idx'} = $ioExpCount;
		$IO_EXP_PIN_DEF{$pinId}->{'chipId'} = &_getColumnIoExp('CHIP_ID');
		$IO_EXP_PIN_DEF{$pinId}->{'idxOfPort'} = &_getColumnIoExp('PIN_INDEX');
		$ioExpCount ++;
	} else {
		delete $IO_EXP_PIN_DEF{$pinId};
	}
}

sub loadIoExpTable ()
{
	unless (defined $IOEXP && -e $IOEXP) {
		return;
	}

	open CSV, "<$IOEXP" or die "Cannot open $IOEXP for read.\n";
	my @csvFile = <CSV>;
	close CSV;

	my $step = 1;
	for (my $i = 0; $i <= $#csvFile; $i += $step) {
		my $line = $csvFile[$i];
		chomp $line;

		$step = 1;
		if ($line !~ /^U[0-9]+.*,/) {
			next;
		}

		# handle multi-line comments
		if ($line =~ /^U[0-9]+.*,/ && $line =~ /,"([^"]*)$/) {
			$step = 0;
			while ($i + $step < $#csvFile) {
				$step ++; 
				my $nextline = $csvFile[$i + $step];
				chomp $nextline;
				my $tmp = $nextline;
				$tmp =~ s/[^,]+//g;
				if (length($tmp) < 8) {
					# this is multi-line comments
					$line .= $nextline;
				} else {
					last;
				}
			}
		}

		&_splitCsvLine($line);
		&insertIoExpCsv2Hash();
	}
}

sub _Int2b17String ()
{
	my $u32Setting = shift;
	my $b17Setting = "";

	for (my $i = 0; $i < 17; $i ++) {
		if ($i % 4 == 0 && $i != 0) {
			$b17Setting = "," . $b17Setting;
		}
		$b17Setting = (($u32Setting & (0x01 << $i)) ? "1" : "0") . $b17Setting;
	}
	
	return $b17Setting;
}

sub _Int2b32String ()
{
	my $u32Setting = shift;
	my $b32Setting = "";

	for (my $i = 0; $i < 32; $i ++) {
		if ($i % 4 == 0 && $i != 0) {
			$b32Setting = "," . $b32Setting;
		}
		$b32Setting = (($u32Setting & (0x01 << $i)) ? "1" : "0") . $b32Setting;
	}
	
	return $b32Setting;
}

sub calcPinDefinition()
{

	#
	# go through the table of IO_EXP
	#
	foreach my $key (sort { $IO_EXP_PIN_DEF{$a}->{idx} <=> $IO_EXP_PIN_DEF{$b}->{idx} } keys %IO_EXP_PIN_DEF) {
		my $thisPin = $IO_EXP_PIN_DEF{$key};
		my $pinName = $thisPin->{NET_NAME};
		
		my $i2cPort = 0x0F;
		if ($thisPin->{I2C_PORT} =~ /^I2C([\d]+)$/) {
			$i2cPort = $1;
			$thisPin->{I2C_PORT} = $i2cPort;
		} else {
			die "Unexpected I2C_PORT of IO_EXP (".$thisPin->{idx}.") $pinName: ". $thisPin->{I2C_PORT};
		}

		my $i2cAddr = 0xFF;
		if ($thisPin->{SLAVE_ADDR} =~ /0x([A-Fa-f0-9]+)/) {
			$i2cAddr = hex $1;
			$thisPin->{SLAVE_ADDR} = $i2cAddr;
		} else {
			die "Unexpected SLAVE_ADDR of IO_EXP (".$thisPin->{idx}.") $pinName: ". $thisPin->{SLAVE_ADDR};
		}

		my $pinNum = 0;
		if ($thisPin->{PIN_INDEX} =~ /^([\d]+)$/) {
			$pinNum = int $1;
			$thisPin->{PIN_INDEX} = $pinNum;
		} else {
			die "Unexpected PIN_INDEX of IO_EXP (".$thisPin->{idx}.") $pinName: ". $thisPin->{PIN_INDEX};
		}
		
		my $i2cSpd = 2;
		if ($thisPin->{I2C_SPEED} =~ /^\s*1M\s*$/) {
			$i2cSpd = 0;
			$thisPin->{I2C_SPEED} = "CLK_1M";
		} elsif ($thisPin->{I2C_SPEED} =~ /^\s*400K\s*$/) {
			$i2cSpd = 1;
			$thisPin->{I2C_SPEED} = "CLK_400K";
		} elsif ($thisPin->{I2C_SPEED} =~ /^\s*100K\s*$/) {
			$i2cSpd = 2;
			$thisPin->{I2C_SPEED} = "CLK_100K";
		} elsif ($thisPin->{I2C_SPEED} =~ /^\s*85K\s*$/) {
			$i2cSpd = 3;
			$thisPin->{I2C_SPEED} = "CLK_85K";
		} elsif ($thisPin->{I2C_SPEED} =~ /^\s*75K\s*$/) {
			$i2cSpd = 4;
			$thisPin->{I2C_SPEED} = "CLK_75K";
		} elsif ($thisPin->{I2C_SPEED} =~ /^\s*65K\s*$/) {
			$i2cSpd = 5;
			$thisPin->{I2C_SPEED} = "CLK_65K";
		} elsif ($thisPin->{I2C_SPEED} =~ /^\s*50K\s*$/) {
			$i2cSpd = 6;
			$thisPin->{I2C_SPEED} = "CLK_50K";
		} else {
			die "Unexpected I2C_SPEED of IO_EXP (".$thisPin->{idx}.") $pinName: ". $thisPin->{I2C_SPEED};
		}
		
		my $PorSetting = "";
		if ($thisPin->{BUF_TYPE} =~ /^\s*I\s*$/) {
			if ($thisPin->{INT_EN} =~ /YES/i) {
				$PorSetting = "ex_IN_INTR";
			} else {
				$PorSetting = "ex_INPUT";
			}
			$thisPin->{POR_SETTING} = "x";
			$thisPin->{BUF_TYPE} = "I";
		} elsif ($thisPin->{BUF_TYPE} =~ /^\s*OD\s*$/) {
			if ($thisPin->{POR_SETTING} =~ /HIGH/i || $thisPin->{POR_SETTING} == 1) {
				$PorSetting = "ex_OD_1";
				$thisPin->{POR_SETTING} = 1;
			} else {
				$PorSetting = "ex_OD_0";
				$thisPin->{POR_SETTING} = 0;
			}
			$thisPin->{BUF_TYPE} = "OD";
		} elsif ($thisPin->{BUF_TYPE} =~ /^\s*O\s*$/) {
			if ($thisPin->{POR_SETTING} =~ /HIGH/i || $thisPin->{POR_SETTING} == 1) {
				$PorSetting = "ex_O_1";
				$thisPin->{POR_SETTING} = 1;
			} else {
				$PorSetting = "ex_O_0";
				$thisPin->{POR_SETTING} = 0;
			}
			$thisPin->{BUF_TYPE} = "O";
		} else {
			die "Unexpected BUF_TYPE of IO_EXP (".$thisPin->{idx}.") $pinName: ". $thisPin->{BUF_TYPE};
		}

		# [31:25] - SN             7-bit
		# [24:21] - I2C port       4-bit
		# [20:18] - I2C speed      3-bit
		#           0 - 1M; 1 - 400K; 2 - 100K; 3 - 85K; 4 - 75K; 5 - 65K; 6 - 50K
		# [17]    - reserved
		# [16]    - POR setting  (HIGH/LOW)
		# [15:14] - Additional pin type (Native/Dummy/Null/IoExp)
		# [13:7]  - I2C slave addr 7-bit
		# [6:5]   - 2'b00 Input; 
		#           2'b01 Input+INT_EN;
		#           2'b10 Output
		#           2'b11 Open Drain
		# [4:0]   - PIN index      5-bit
		$thisPin->{PIN_CTRL_MACRO} = sprintf("#define %-24s ( _SN(%3d) | IO_EXP_FLG | %-10s | (I2C_%d << 21) | (%d << 18) | (0x%02X << 7) | %2d )    // %s", 
						$thisPin->{srcNET_NAME}, $thisPin->{idx} & 0x7F, $PorSetting, $i2cPort & 0x0F, $i2cSpd & 0x07, $i2cAddr & 0x7F, $pinNum, $thisPin->{CHIP_ID} . ", " . sprintf("%2d", $pinNum) . " | " . $thisPin->{DESCRIPTION});
	}
	
	#
	# %IO_EXP_INIT_REG
	#
	foreach my $key (sort { $IO_EXP_PIN_DEF{$a}->{idx} <=> $IO_EXP_PIN_DEF{$b}->{idx} } keys %IO_EXP_PIN_DEF) {
		my $thisPin = $IO_EXP_PIN_DEF{$key};
		my $pinName = $thisPin->{NET_NAME};
		my $chipId = $thisPin->{CHIP_ID};
		my $pinNum = $thisPin->{PIN_INDEX};
		
		my $chipName = $chipId."_I2C".$thisPin->{I2C_PORT}."_0x".sprintf ("%02X", $thisPin->{SLAVE_ADDR});
		
		if (not exists $IO_EXP_INIT_REG{$chipName}) {
			my %tmp = ();
			$IO_EXP_INIT_REG{$chipName} = \%tmp;
			
			$IO_EXP_INIT_REG{$chipName}->{OUTPUT_MASK} = 0;
			$IO_EXP_INIT_REG{$chipName}->{OD_MASK} = 0;
			$IO_EXP_INIT_REG{$chipName}->{POR_VAL} = 0;
			
			$IO_EXP_INIT_REG{$chipName}->{I2C_PORT} = $thisPin->{I2C_PORT};
			$IO_EXP_INIT_REG{$chipName}->{SLAVE_ADDR} = $thisPin->{SLAVE_ADDR};
			$IO_EXP_INIT_REG{$chipName}->{I2C_SPEED} = $thisPin->{I2C_SPEED};
			
			$IO_EXP_INIT_REG{$chipName}->{HELPER_INFO} = "/*\n * " . $chipId. ", Slv_0x". sprintf("%02X, ", $IO_EXP_INIT_REG{$chipName}->{SLAVE_ADDR}) . "I2C_" .$IO_EXP_INIT_REG{$chipName}->{I2C_PORT} ."\n";
			$IO_EXP_INIT_REG{$chipName}->{HELPER_INFO} .= " *\n *        NetName                    BufType  POR_Def  Comment\n";

			$IO_EXP_INIT_REG{$chipName}->{ECNAME_INFO} = "  ----,8,         // " . $chipId. ", Slv_0x". sprintf("%02X, ", $IO_EXP_INIT_REG{$chipName}->{SLAVE_ADDR}) . "I2C_" .$IO_EXP_INIT_REG{$chipName}->{I2C_PORT} ."\n";
			$IO_EXP_INIT_REG{$chipName}->{ECNAME_INFO} .= "                  //                NetName                    Type POR  Comment\n"
		}
		
		if ($thisPin->{BUF_TYPE} =~ /^\s*OD\s*$/) {
			$IO_EXP_INIT_REG{$chipName}->{OD_MASK} |= (0x01 << $pinNum);
		} elsif ($thisPin->{BUF_TYPE} =~ /^\s*O\s*$/) {
			$IO_EXP_INIT_REG{$chipName}->{OUTPUT_MASK} |= (0x01 << $pinNum);
		}
		
		if ( ($thisPin->{BUF_TYPE} =~ /^\s*OD\s*$/ || $thisPin->{BUF_TYPE} =~ /^\s*O\s*$/) &&
			 1 == $thisPin->{POR_SETTING} ) {
			$IO_EXP_INIT_REG{$chipName}->{POR_VAL} |= (0x01 << $pinNum);
		}
		
		if ( ($thisPin->{INT_EN} =~ /YES/i )) {
			$IO_EXP_INIT_REG{$chipName}->{INT_MASK} |= (0x01 << $pinNum);
		}

		$IO_EXP_INIT_REG{$chipName}->{HELPER_INFO} .= sprintf(" * IO_%02d  %-25s  %-2s       %s        %s\n", $pinNum, $pinName, $thisPin->{BUF_TYPE}, $thisPin->{POR_SETTING}, $thisPin->{DESCRIPTION});
		$IO_EXP_INIT_REG{$chipName}->{ECNAME_INFO} .= sprintf("                  // Bit[%d] - P%d.%d  %-25s  %-2s    %s    %s\n", $pinNum % 8, $pinNum / 8, $pinNum % 8, $pinName, $thisPin->{BUF_TYPE}, $thisPin->{POR_SETTING}, $thisPin->{DESCRIPTION});
	}
}

sub loadRefHeaderFile ()
{
	open H, "<$REFHFILE" or die "Cannot open $REFHFILE for read.\n";
	while (my $line = <H>) {
		chomp $line;
		if ($line =~ /^\s*#define\s+ADDR_GPIO_GPIO(\d+)_PIN_CONTROL\s+\(?0x([A-Fa-f0-9+-]+)\s*\)?\s*$/) {
			my $strPinIdx = sprintf("%03d", $1);
			my $strOffset = $2;
			my $intOffset = 0;
			if ($strOffset =~ /\+/) {			# Workaround to fix offset issue.  0x40081280+4
				my @tmp = split /\s*\+\s*/, $strOffset;
				$intOffset = hex ($tmp[0]) + $tmp[1];
			} elsif ($strOffset =~ /\-/) {		# Workaround to fix offset issue.  0x40081280-4
				my @tmp = split /\s*\+\s*/, $strOffset;
				$intOffset = hex ($tmp[0]) - $tmp[1];
			} else {
				$intOffset = hex $strOffset;
			}
			my $mmcr = $intOffset;
			die "Duplicated define of GPIO$strPinIdx, was $GPIO_REG{$strPinIdx}, new $2 \n" if (exists $GPIO_REG{$strPinIdx});
			$GPIO_REG{$strPinIdx} = $mmcr;
		}
	}
	close H;
}

sub genCopyright
{
	my $handler = shift;
	my $fileName = shift;
	my $desp = shift;
	
	print $handler "/*****************************************************************************\n";
	print $handler " * Copyright (C) 2023 Advanced Micro Devices, Inc. All rights reserved.\n";
	print $handler " ******************************************************************************\n";
	print $handler " */";
}

sub genHeader
{
	my $handler = shift;

	printf $handler "\n#ifndef __BOARD_IOEXPTABLE_H__\n";
	printf $handler "#define __BOARD_IOEXPTABLE_H__\n\n";
	
	print $handler "#include \"i2c_hub.h\"\n";
}

sub genTail
{
	my $handler = shift;
	printf $handler "\n#endif // end of __BOARD_IOEXPTABLE_H__\n";
}

sub _genRuntimeSwitchMacros ()
{
	my $st = shift;
	my $buf = "";
	# TODO: description of runtime switching
	foreach my $key (sort { $PIN_DEF{$a}->{PIN_NAME} cmp $PIN_DEF{$b}->{PIN_NAME} } keys %PIN_DEF) {
		my $thisPin = $PIN_DEF{$key};
		if ($thisPin->{$st} !~ /^\s*$/) {
			if ($thisPin->{$st} =~ /\s*([0-9A-Fa-f]+)h\/([0-9A-Fa-f]+)h\s*$/) {
				my $val = $1;
				my $msk = $2;
				$buf .= sprintf "    { %-40s, 0x%05X, 0x%05X },    \\\n", $thisPin->{srcNET_NAME}, hex $val, hex $msk;
			} else {
				print "[ERROR] Incorrect RUNTIME STATUS $st ($thisPin->{$st}) of $thisPin->{PIN_NAME}. Skipped ...\n";
			}
		}
	}
	$buf .= "    { GPIO_PIN_NULL                           , 0x00000, 0x00000 }";
	$buf = sprintf "#define GPIO_RUNTIME_SWITCH_%-5s                                      \\\n". $buf . "\n\n", $st;
	return $buf;
}

sub autoGenHeaderFile()
{
	my $handler = shift;

	my $pinNameDbgString = "";
	my %hashBuf = ();
	foreach my $key (sort { $PIN_DEF{$a}->{PIN_NAME} cmp $PIN_DEF{$b}->{PIN_NAME} } keys %PIN_DEF) {
		my $thisPin = $PIN_DEF{$key};
		if ($thisPin->{PIN_NAME} =~ /GPIO(\d{2})(\d)/) {
			my $grpIdx = $1;
			my $pinIdx = $2;
			
			if (not exists $hashBuf{$grpIdx}) {
				my %t = ();
				$hashBuf{$grpIdx} = \%t;
			}
			if (not exists $hashBuf{$grpIdx}->{$pinIdx}) {
				my %t = ();
				$hashBuf{$grpIdx}->{$pinIdx} = \%t;
			}
			
			my @funs = split /\s*\/\s*/, $thisPin->{PIN_NAME};
			my $comments = " * ";
			for (my $i = 1; $i <= 6; $i++) {
				my $fkey = "FUN".$i;
				my $pkey = "PAD".$i;
				if ($thisPin->{$fkey} ne "" && $thisPin->{$pkey} ne "") {
					$comments .= sprintf "%-18s ", $thisPin->{$fkey}."/".$thisPin->{$pkey};
				} elsif ($thisPin->{$fkey} ne "") {
					$comments .= sprintf "%-18s ", $thisPin->{$fkey};
				} else {
					$comments .= sprintf "%-18s ", "------------";
				}
			}
			if ($thisPin->{dftFUN} ne "" && $thisPin->{dftPAD} ne "") {
				$comments .= sprintf "%-18s", $thisPin->{dftFUN}."/".$thisPin->{dftPAD};
			} elsif ($thisPin->{dftFUN} ne "") {
				$comments .= sprintf "%-18s", $thisPin->{dftFUN};
			} else {
				$comments .= sprintf "%-18s", "------------";
			}
			
			$hashBuf{$grpIdx}->{$pinIdx}->{COMMENT} = $comments;
			$hashBuf{$grpIdx}->{$pinIdx}->{DEFINE} = $thisPin->{PIN_CTRL_MACRO} . "    // ".$thisPin->{SETTING_DESCRIPTION};
			
			$hashBuf{$grpIdx}->{$pinIdx}->{THE_PIN} = $thisPin;
		}
		
	}
	
	my $fileBuf = "";
	
	if (scalar keys %IO_EXP_PIN_DEF > 0) {
		$fileBuf .= "\n";
		$fileBuf .= "#define _SN(x)        (((x) & 0x7Ful) << 25)\n";
		$fileBuf .= "#define PSN(p)        (((p) >> 25) & 0x7F)\n";
		$fileBuf .= "#define ex_OD_1    ((1 << 16) | (3 << 5))  // [6:5]   2'b11 Open Drain\n";
		$fileBuf .= "#define ex_OD_0    ((0 << 16) | (3 << 5))  // [16]    POR setting  (HIGH/LOW)\n";
		$fileBuf .= "#define ex_O_1     ((1 << 16) | (2 << 5))  // [6:5]   2'b10 Output\n";
		$fileBuf .= "#define ex_O_0     ((0 << 16) | (2 << 5))  // [16]    POR setting  (HIGH/LOW)\n";
		$fileBuf .= "#define IO_EXP_FLG    (3ul << 14)            // 0 - Native; 2 - Null; 3 - IO_EXP\n";
		$fileBuf .= "#define ex_IN_INTR              (1 << 5)   // [6:5]   2'b01 Input+INT_EN\n";
		$fileBuf .= "#define ex_INPUT                (0 << 5)   // [6:5]   2'b00 Input\n";
		$fileBuf .= "\n";
		$fileBuf .= "/*      ex NET NAME                    SEQ  |            | BUF_TYPE   |  I2C PORT  | I2C SPEED | SLAVE ADDR  | idx */\n";
		#
		# go through the table of IO_EXP
		#
		my $curPort = 0;
		my $curChip = 0;
		foreach my $key (sort { $IO_EXP_PIN_DEF{$a}->{idx} <=> $IO_EXP_PIN_DEF{$b}->{idx} } keys %IO_EXP_PIN_DEF) {
			my $thisPin = $IO_EXP_PIN_DEF{$key};
			my $pinName = $thisPin->{NET_NAME};
			
			if ($curPort != int $thisPin->{'idxOfPort'} / 8 || $curChip != $thisPin->{'chipId'}) {
				$fileBuf .= "\n";

				$curPort = int $thisPin->{'idxOfPort'} / 8;
				$curChip = $thisPin->{'chipId'};
			}
			
			$fileBuf .= $thisPin->{PIN_CTRL_MACRO}."\n";
			$pinNameDbgString .= sprintf "    %-40s   /* %3d */   \\\n", "\"".$thisPin->{NET_NAME}."\",", $thisPin->{idx};
		}

		$fileBuf .= "\n";
		foreach my $key (sort keys %IO_EXP_INIT_REG) {
			my $maskTable = $IO_EXP_INIT_REG{$key};
			my $OutputMask = $maskTable->{OUTPUT_MASK};
			my $ODMask = $maskTable->{OD_MASK};
			my $PorVal = $maskTable->{POR_VAL};
			
			$fileBuf .= $maskTable->{HELPER_INFO};
			$fileBuf .= " */\n";
			$fileBuf .= sprintf "#define IOEXP_MASK_O___%-25s   (0x%08Xul)  // %s\n", $key,  $maskTable->{OUTPUT_MASK}, &_Int2b32String($maskTable->{OUTPUT_MASK});
			$fileBuf .= sprintf "#define IOEXP_MASK_OD__%-25s   (0x%08Xul)  // %s\n", $key,  $maskTable->{OD_MASK}, &_Int2b32String($maskTable->{OD_MASK});
			$fileBuf .= sprintf "#define IOEXP_MASK_POR_%-25s   (0x%08Xul)  // %s\n", $key,  $maskTable->{POR_VAL}, &_Int2b32String($maskTable->{POR_VAL});
			$fileBuf .= sprintf "#define IOEXP_MASK_INT_%-25s   (0x%08Xul)  // %s\n", $key,  $maskTable->{INT_MASK}, &_Int2b32String($maskTable->{INT_MASK});
			$fileBuf .= sprintf "#define IOEXP_I2C_PORT_%-25s   (I2C_%d)         //    Port: I2C_%d\n", $key,  $maskTable->{I2C_PORT}, $maskTable->{I2C_PORT};
			$fileBuf .= sprintf "#define IOEXP_I2C_ADDR_%-25s   (0x%02X)          // SlvAddr: (0x%02X << 1), 8-bit: 0x%02X\n", $key,  $maskTable->{SLAVE_ADDR}, $maskTable->{SLAVE_ADDR}, $maskTable->{SLAVE_ADDR} << 1;
			$fileBuf .= "\n";
		}
	}
	
	printf $handler $fileBuf;
}



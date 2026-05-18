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
my %colName2Idx = (
#   Column Name      Column Index
	PIN              => 'A',
	PIN_NAME         => 'B',
	PWR_RAIL         => 'C',
	
	GIRQ_GROUP       => 'R',
	GIRQ_BIT         => 'S',
	
	FUN1             => 'D',
	FUN2             => 'E',
	FUN3             => 'F',
	FUN4             => 'G',
	FUN5             => 'H',
	FUN6             => 'I',
	PAD1             => 'L',
	PAD2             => 'M',
	PAD3             => 'N',
	PAD4             => 'O',
	PAD5             => 'P',
	PAD6             => 'Q',

	dftFUN           => 'J',
	dftPAD           => 'K',
	
	AutoGen          => 'U',
	NET_NAME         => 'V',
	CATEGORY         => 'W',
	SELECTD_FUN      => 'X',
	INTERNAL_PAD     => 'Y',
	DIR_VAL          => 'Z',
	INT_TYPE         => 'AA',
	PIN_PWR          => 'AB',

	G3               => 'AC',
	S5S4             => 'AD',
	S3               => 'AE',
	S0               => 'AF',

	ACPI_GROUP       => 'AG',
	ACPI_IDX         => 'AH',

	INT_HANDLER      => 'AI',
	DESCRIPTION      => 'AN',
	
# Modular card info
	SLOT_NAME        => 'AJ',
	SLOT_IDX         => 'AK',
	MDU_PIN_NAME     => 'AL',
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
my $INPUT;    # the main table
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
	# &loadRefHeaderFile();
	&loadCsv();
	&calcPinDefinition();
	
	my $fh;
	open $fh, ">".$OUTDIR.$headerFile or die "Cannot open $OUTDIR$headerFile for write.\n";
	&genCopyright($fh, $headerFile, "Project special defination for $PROGRAM");
	&genHeader($fh);
	&autoGenHeaderFile($fh);
	&genTail($fh);
	close $fh;

	open $fh, ">".$OUTDIR.$gpioAutoGen or die "Cannot open $OUTDIR$gpioAutoGen for write.\n";
	&genCopyright($fh, $gpioAutoGen, "AutoGen for GPIO");
	&autoGenSourceFile($fh);
	close $fh;

	my @bufH = ();
	open $fh, "<".$OUTDIR.$headerFile or die "Cannot open $OUTDIR$headerFile for read.\n";
	@bufH = <$fh>;
	close $fh;

	my @bufC = ();
	open $fh, "<".$OUTDIR.$gpioAutoGen or die "Cannot open $OUTDIR$gpioAutoGen for read.\n";
	@bufC = <$fh>;
	close $fh;

	print "\n !!!DONE!!!\n\n";
	printf "# %4d: $OUTDIR.$headerFile\n", $#bufH;
	printf "# %4d: $OUTDIR.$gpioAutoGen\n", $#bufC;
	print "\n\n";
}

#######################
sub initCmdEnv
{
	# get commandline options
	&GetOptions (	'outdir=s' => \$OUTDIR,
					'maincsv=s' => \$INPUT,
					'program=s' => \$PROGRAM,
				) or die $!;

	$OUTDIR =~ s/[\\\/]\s*$//;
	$OUTDIR =~ s/[\\\/]/\//;
	$OUTDIR .= "/";
	die "$OUTDIR is unavailable.\n" if (not -d $OUTDIR);
	
	die "Undefined program name.\n" if (not defined $PROGRAM);
	$headerFile = "gpioAutoGen.h";
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

sub insertCsv2Hash
{
	my $pin = &_getColumn('PIN');
	
	die "Duplicated define of $pin from $INPUT.\n" if (exists $PIN_DEF{$pin});
	
	foreach my $key (keys %colName2Idx) {
		$PIN_DEF{$pin}->{$key} = &_getColumn($key);
	}

	# override NET NAME
	my $netName = $PIN_DEF{$pin}->{NET_NAME};
	$netName =~ s/#/_N/g;
	$netName =~ s/[^A-Za-z0-9_]/_/g;
	$netName =~ s/_*$//g;
	$PIN_DEF{$pin}->{'srcNET_NAME'} = $netName;
}

sub loadCsv ()
{
	open CSV, "<$INPUT" or die "Cannot open $INPUT for read.\n";
	my @csvFile = <CSV>;
	close CSV;

	my $step = 1;
	for (my $i = 0; $i <= $#csvFile; $i += $step) {
		my $line = $csvFile[$i];
		chomp $line;
		
		$step = 1;
		if ($line !~ /^[A-N][0-9]+,/) {
			next;
		}
=cut
		if ($line =~ /GPIO045/) {
			print "DEBUG\n";
		}
=cut
		# handle multi-line comments
		if ($line =~ /^[A-N][0-9]+,/ && $line =~ /,"([^"]*)$/) {
			$step = 0;
			while ($i + $step < $#csvFile) {
				$step ++; 
				my $nextline = $csvFile[$i + $step];
				chomp $nextline;
				my $tmp = $nextline;
				$tmp =~ s/[^,]+//g;
				if (length($tmp) < 10) {
					# this is multi-line comments
					$line .= $nextline;
				} else {
					last;
				}
			}
		}
		
		&_splitCsvLine($line);
		insertCsv2Hash();
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
	# print the hint message for the skipped PINs.
	foreach my $key (sort { $PIN_DEF{$a}->{PIN_NAME} cmp $PIN_DEF{$b}->{PIN_NAME} } keys %PIN_DEF) {
		if ($PIN_DEF{$key}->{AutoGen} eq 'NO') {
			print $PIN_DEF{$key}->{PIN_NAME}." is skipped !!! AutoGen flag is 'NO'\n";
		}
	}

	# init POR setting and GPIO/BGPO macros
	foreach my $key (sort { $PIN_DEF{$a}->{PIN_NAME} cmp $PIN_DEF{$b}->{PIN_NAME} } keys %PIN_DEF) {
		my $thisPin = $PIN_DEF{$key};
		my $selfun = $thisPin->{SELECTD_FUN};
		my $pinName = $thisPin->{PIN_NAME};
		
		my @supportedFunctions = split /\s*\/\s*/, $pinName;
		
		if ($selfun =~ /^\s*$/) {
			if ($#supportedFunctions == 0) {
				# single function
				$thisPin->{SELECTD_FUN} = $supportedFunctions[0];
				$selfun = $thisPin->{SELECTD_FUN};
			}
		} elsif ($pinName !~ m/$selfun/i && $thisPin->{AutoGen} ne 'NO') {
			print "[WARNING] The selected function ($selfun) is mismatched with $pinName, skipped !!!\n";
			$thisPin->{AutoGen} = 'NO';
			next;
		}
		
		# Collect BGPO
		if ($selfun =~ /BGPO/i && $pinName =~ /BGPO(\d)/) {
			my $idx = $1;
			if (not exists $BGPO_TAB{$idx}) {
				my %oneItem = ();
				$BGPO_TAB{$idx} = \%oneItem;
			}
			
			# !!NOTE!! 
			# There's no special column for BGPO power, so it's leveraging GPIO 'PIN POWER'.
			# PWR_OFF := power off both GPIO and BGPO
			# for other values, keeps BGPO power on.
			if ($thisPin->{PIN_PWR} =~ /PWR_OFF/) {
				$BGPO_TAB{$idx}->{POR_PWR} = 0;
			} else {
				$BGPO_TAB{$idx}->{POR_PWR} = 1;
			}
			
			# BGPO is default OUTPUT LOW.
			$BGPO_TAB{$idx}->{VAL} = 0;
			if ($thisPin->{DIR_VAL} =~ /O(D)?_(\d)/) {
				$BGPO_TAB{$idx}->{VAL} = $1;
			}
		}
		
		# Override I2C settings 
		# if used as I2C, set as OD_1 no matter POR INITIAL/INTERNAL PAD is defined or not.
		# report warning if conflict with these defination.
		# the final setting is following the setup of cvs.
		if ($selfun =~ /I2C/) {
			if ($thisPin->{DIR_VAL} =~ /^\s*$/) {
				print "[INFO] Set $pinName as OD_1 as it used for $selfun.\n";
				$thisPin->{DIR_VAL} = "OD_1";
			} elsif ($thisPin->{DIR_VAL} !~ /OD_1/) {
				print "[WARNING] $pinName is used for $selfun but DIR_VAL is $thisPin->{INTERNAL_PAD} !!!\n";
			}
			if ($thisPin->{INTERNAL_PAD} =~ /PD/) {
				print "[WARNING] $pinName is used for $selfun but INTERNAL_PAD is PD !!!\n";
			}
			if ($thisPin->{INT_TYPE} !~ /^\s*$/ && $thisPin->{INT_TYPE} !~ /I(NT)?(_)?[ODN]/) {
				print "[WARNING] $pinName is used for $selfun but INT_TYPE is $thisPin->{INT_TYPE} !!!\n";
			}
		}
		
		# go through all GPIO
		if ($pinName =~ /GPIO(([A-Fa-f0-9+-]+){2})/) {
			#print $PIN_DEF{$key}->{CATEGORY}."   ".$PIN_DEF{$key}->{PIN_NAME}."\n";
print "IDX =  $1!!!\n";
			my $idx = $1;

			my $u32Setting = 0;
			my $strSetting = "";
			my $strBrief = "";
			
			# function
			my $funId = 0;
			my $theFun = $thisPin->{SELECTD_FUN};
			if ($theFun =~ /^\s*$/) {
				$thisPin->{SELECTD_FUN} = 'GPIO';
				$funId = 0;
			} elsif ($theFun =~ /FUN(\d)\s*$/) {
				my $i = $1;
				if ($i >= 0 && $i <= $#supportedFunctions) {
					$thisPin->{SELECTD_FUN} = $supportedFunctions[$i];
					$funId = $i;
				} else {
					print "[WARNING] Invalid function ($theFun) assigned to $pinName. Set as GPIO ...\n";
				}
			} else {
				$thisPin->{SELECTD_FUN} = "";
				foreach my $fun (@supportedFunctions) {
					if ($fun =~ /$theFun/i) {
						$thisPin->{SELECTD_FUN} = $fun;
						last;
					}
					$funId ++;
				}
				if ($thisPin->{SELECTD_FUN} eq "") {
					$funId = 0;
					$thisPin->{SELECTD_FUN} = "GPIO";
					print "[WARNING] Invalid function ($theFun) assigned to $pinName. Set as GPIO ...\n";
				}
			}
			$u32Setting &= ~(Bit12 | Bit13 | Bit14);
			$u32Setting |= ($funId << 12);
			$strSetting .= sprintf ("%-12s", $thisPin->{SELECTD_FUN});
			
			# pin pad
			my $pinPad = $thisPin->{INTERNAL_PAD};
			$u32Setting &= ~(Bit0 + Bit1);
			if ($pinPad =~ /KEEP/i) {
				$u32Setting |= 3;
				# $strSetting .= "PAD_KEEP, ";
				$strBrief   .= "Kp, ";
				$thisPin->{INTERNAL_PAD_STR} = sprintf ("");
			} elsif ($pinPad =~ /PD/i) {
				$u32Setting |= 2;
				# $strSetting .= "PAD_PD  , ";
				$strBrief   .= "PD, ";
				$thisPin->{INTERNAL_PAD_STR} = sprintf (" | GPIO_PULL_DOWN");
			} elsif ($pinPad =~ /PU/i) {
				$u32Setting |= 1;
				# $strSetting .= "PAD_PU  , ";
				$strBrief   .= "PU, ";
				$thisPin->{INTERNAL_PAD_STR} = sprintf (" | GPIO_PULL_UP");
			} elsif ($pinPad =~ /NONE/ || $pinPad =~ /^\s*$/) {
				$u32Setting |= 0;
				# $strSetting .= "PAD_None, ";
				$strBrief   .= "--, ";
				$thisPin->{INTERNAL_PAD_STR} = sprintf ("");
			} else {
				print "[WARNING] Invalid pin PAD ($pinPad) assigned to $pinName. Set as NONE ...\n";
				$u32Setting |= 0;
				# $strSetting .= "PAD_None, ";
				$strBrief   .= "--, ";
				$thisPin->{INTERNAL_PAD_STR} = sprintf ("");
			}
			
			# GPIO direction and value
			$u32Setting &= ~Bit16;
			if ($thisPin->{DIR_VAL} =~ /O(D)?_(\d)/) {
				if ($2 == 1){
					$u32Setting |= Bit16 ;
				$thisPin->{VAL_STR} = sprintf ("GPIO_OUTPUT_HIGH");
				} else {
				$thisPin->{VAL_STR} = sprintf ("GPIO_OUTPUT_LOW");
				}
			}

			if ($thisPin->{DIR_VAL} =~ /OD/) {
				$u32Setting |= (3 << 8);
				$thisPin->{DIR_STR} = sprintf (" | GPIO_OPEN_DRAIN");
			} elsif ($thisPin->{DIR_VAL} =~ /O_/) {
				$u32Setting |= (2 << 8);
				$thisPin->{DIR_STR} = sprintf ("");
			} else {
				$thisPin->{DIR_VAL} = "IN";
				$thisPin->{DIR_STR} = sprintf ("GPIO_INPUT");
			}
			# $strSetting .= sprintf ("%-4s, ", $thisPin->{DIR_VAL});
			$strBrief   .= sprintf ("%-4s", $thisPin->{DIR_VAL});

			# interrupt
			my $intType = $thisPin->{INT_TYPE};
			$u32Setting &= ~(Bit4 | Bit5 | Bit6 | Bit7);
			if ($intType =~ /RISING/i) {
				$u32Setting |= (0x0D << 4);
				# $strSetting .= "GPIO_INT_EDGE_RISING , ";
				$thisPin->{INT_TYPE_STR} = sprintf (" | GPIO_INT_EDGE_RISING");
			} elsif ($intType =~ /FALLING/i) {
				$u32Setting |= (0x0E << 4);
				# $strSetting .= "GPIO_INT_EDGE_FALLING, ";
				$thisPin->{INT_TYPE_STR} = sprintf (" | GPIO_INT_EDGE_FALLING");
			} elsif ($intType =~ /BOTH/i) {
				$u32Setting |= (0x0F << 4);
				# $strSetting .= "GPIO_INT_EDGE_BOTH   , ";
				$thisPin->{INT_TYPE_STR} = sprintf (" | GPIO_INT_EDGE_BOTH");
			} elsif ($intType =~ /HIGH/i) {
				$u32Setting |= (0x01 << 4);
				# $strSetting .= "GPIO_INT_LEVEL_HIGH   , ";
				$thisPin->{INT_TYPE_STR} = sprintf (" | GPIO_INT_LEVEL_HIGH");
			} elsif ($intType =~ /LOW/i) {
				$u32Setting |= (0x00 << 4);
				# $strSetting .= "GPIO_INT_LEVEL_LOW    , ";
				$thisPin->{INT_TYPE_STR} = sprintf (" | GPIO_INT_LEVEL_LOW");
			} elsif ($intType =~ /^I(NT)?(_)?[ODN]/ || $intType =~ /^\s*$/) {
				$u32Setting |= (0x04 << 4);
				# $strSetting .= "INT_None   , ";
			} else {
				print "[WARNING] Invalid interrupt type ($intType) assigned to $pinName. Set as Interrupt disabled ...\n";
				$u32Setting |= (0x04 << 4);
				# $strSetting .= "INT_None   , ";
			}
			
			# pin power
			my $pinPwr = $thisPin->{PIN_PWR};
			$u32Setting &= ~(Bit2 + Bit3);
			if ($pinPwr =~ /VTRrel/i) {
				$u32Setting |= (0x03 << 2);
				# $strSetting .= "GPIO_CTRL_PWRG_RSVD, ";
				$thisPin->{PIN_PWR_STR} = sprintf("GPIO_CTRL_PWRG_RSVD");
			} elsif ($pinPwr =~ /VCC/i) {
				$u32Setting |= (0x01 << 2);
				# $strSetting .= "GPIO_CTRL_PWRG_VCC_IO  , ";
				$thisPin->{PIN_PWR_STR} = sprintf("GPIO_CTRL_PWRG_VCC_IO");
			} elsif ($pinPwr =~ /PWR_OFF/i) {
				$u32Setting |= (0x02 << 2);
				# $strSetting .= "GPIO_CTRL_PWRG_OFF   , ";
				$thisPin->{PIN_PWR_STR} = sprintf("GPIO_CTRL_PWRG_OFF");
			} elsif ($pinPwr =~ /VTR/ || $pinPwr =~ /^\s*$/) {
				$u32Setting |= (0x00 << 2);
				# $strSetting .= "GPIO_CTRL_PWRG_VTR_IO, ";
				$thisPin->{PIN_PWR_STR} = sprintf("GPIO_CTRL_PWRG_VTR_IO");
			} else {
				print "[WARNING] Invalid pin power ($pinPwr) assigned to $pinName. Set as VTR power ...\n";
				$u32Setting |= (0x00 << 2);
				# $strSetting .= "GPIO_CTRL_PWRG_VTR_IO, ";
				$thisPin->{PIN_PWR_STR} = sprintf("GPIO_CTRL_PWRG_VTR_IO");
			}
			
			$strSetting =~ s/,\s*$//;
			$thisPin->{SETTING_DESCRIPTION} = $strSetting;
			$thisPin->{SETTING_BRIEF} = $strBrief;
			
			$thisPin->{PIN_CTRL_MACRO} = sprintf("    {%-20s%-60s%-27s},", $thisPin->{srcNET_NAME}.", ", $thisPin->{VAL_STR}.$thisPin->{DIR_STR}.$thisPin->{INT_TYPE_STR}.$thisPin->{INTERNAL_PAD_STR}.", ",$thisPin->{PIN_PWR_STR});
			# # DEBUG OVERRIDE
			# if ($pinName =~ /UART(0)_[TR]X/ && $pinName =~ /GPIO10[45]/) {
			# 	# bit field [15:14] equals 1 meaning dummy pin; Define it as dummp pin so it can keep inactive at runtime.
			# 	$thisPin->{PIN_CTRL_MACRO_DBG_UART} = sprintf("#define cGPIO%03d_CTRL ( _SN(%3d) | (0%03d << 17) | c17bit(%s) )", $idx, $gpioCount, $idx, &_Int2b17String($u32Setting&0xFFF | (0x01 << 12)));
			# }
			# if ($pinName =~ /UART(1)_[TR]X/ && $pinName =~ /GPIO17[01]/) {
			# 	# bit field [15:14] equals 1 meaning dummy pin; Define it as dummp pin so it can keep inactive at runtime.
			# 	$thisPin->{PIN_CTRL_MACRO_DBG_UART} = sprintf("#define cGPIO%03d_CTRL ( _SN(%3d) | (0%03d << 17) | c17bit(%s) )", $idx, $gpioCount, $idx, &_Int2b17String($u32Setting&0XFFF | (0x01 << 12)));
			# }
			# if ($pinName =~ /TRACEDAT[01234]/) {
			# 	# bit field [15:14] equals 1 meaning dummy pin; Define it as dummp pin so it can keep inactive at runtime.
			# 	$thisPin->{PIN_CTRL_MACRO_DBG_TRACE} = sprintf("#define cGPIO%03d_CTRL ( _SN(%3d) | (0%03d << 17) | c17bit(%s) )", $idx, $gpioCount, $idx, &_Int2b17String($u32Setting | (0x03 << 13)));
			# }
			$thisPin->{PIN_SERIAL_NUM} = $gpioCount ++;
			$thisPin->{PIN_OPT_MACRO}  = sprintf("#define %-24s EC_GPIO_%02s", $thisPin->{srcNET_NAME}, $idx);
			$thisPin->{POR_U32_SETTING}= $u32Setting;
			
			# Interrupt handler
			if ($thisPin->{INT_HANDLER} !~ /^\s*$/) {     # found a blank line
				$thisPin->{PIN_INT_HANDLER_MACRO} = sprintf("#define ISR_GPIO%03d     %-30s // POR %s", $idx, $thisPin->{INT_HANDLER}, $intType);
			}
			
#			print $GPIO_TAB{$idx}->{PIN_OPT_MACRO} . " ". $GPIO_TAB{$idx}->{PIN_CTRL_MACRO}."  // ".$strSetting."\n";
			
			# # Check if GPIO REGISTER is correct or not
			# my $mmcr_h = $GPIO_REG{$idx};
			# my $mmcr_calc = 0x40081000;  # GPIO_000_036_BASE / GPIO_BASE
			# $mmcr_calc += ((oct $idx) << 2);
			# if ($mmcr_calc != $mmcr_h) {
			# 	printf "[ERROR] MMCR address of GPIO%03d is incorrect!  Could be 0x%08X but 0x%08X from $REFHFILE\n", $idx, $mmcr_calc, $mmcr_h;
			# }
			# $thisPin->{PIN_MMCR} = $mmcr_h;
			
			# EC ACPI Space
			foreach my $key (sort { $PIN_DEF{$a}->{ACPI_GROUP} cmp $PIN_DEF{$b}->{ACPI_GROUP} } keys %PIN_DEF) {
				if ($PIN_DEF{$key}->{ACPI_GROUP} =~ /GPO([\d]+)$/ || $PIN_DEF{$key}->{ACPI_GROUP} =~ /GPI([\d]+)$/ || $PIN_DEF{$key}->{ACPI_GROUP} =~ /GIO([\d]+)$/) {
					my $gid = $1;
					if (not exists $EC_ACPI_GROUP{$gid}) {
						my %bitMap = ();
						$EC_ACPI_GROUP{$gid} = \%bitMap;
					}
					
					my $bid = $PIN_DEF{$key}->{ACPI_IDX};
					if ($bid < 0 || $bid > 7) {
						print "[ERROR] Incorrect bit assignment of $PIN_DEF{$key}->{PIN_NAME}, ACPI Group/Bit $gid/$bid, skipped ...\n";
					} else {
						$EC_ACPI_GROUP{$gid}->{$bid} = $PIN_DEF{$key};
					}
				}
			}
		}
	}

}

# sub loadRefHeaderFile ()
# {
# 	open H, "<$REFHFILE" or die "Cannot open $REFHFILE for read.\n";
# 	while (my $line = <H>) {
# 		chomp $line;
# 		if ($line =~ /^\s*#define\s+ADDR_GPIO_GPIO(\d+)_PIN_CONTROL\s+\(?0x([A-Fa-f0-9+-]+)\s*\)?\s*$/) {
# 			my $strPinIdx = sprintf("%03d", $1);
# 			my $strOffset = $2;
# 			my $intOffset = 0;
# 			if ($strOffset =~ /\+/) {			# Workaround to fix offset issue.  0x40081280+4
# 				my @tmp = split /\s*\+\s*/, $strOffset;
# 				$intOffset = hex ($tmp[0]) + $tmp[1];
# 			} elsif ($strOffset =~ /\-/) {		# Workaround to fix offset issue.  0x40081280-4
# 				my @tmp = split /\s*\+\s*/, $strOffset;
# 				$intOffset = hex ($tmp[0]) - $tmp[1];
# 			} else {
# 				$intOffset = hex $strOffset;
# 			}
# 			my $mmcr = $intOffset;
# 			die "Duplicated define of GPIO$strPinIdx, was $GPIO_REG{$strPinIdx}, new $2 \n" if (exists $GPIO_REG{$strPinIdx});
# 			$GPIO_REG{$strPinIdx} = $mmcr;
# 		}
# 	}
# 	close H;
# }

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

	printf $handler "\n#ifndef __GPIOAUGOGEN_H__\n";
	printf $handler "#define __GPIOAUGOGEN_H__\n\n";
	
	print $handler "#include <zephyr/kernel.h>\n";
	print $handler "#include <system.h>\n";
}

sub genTail
{
	my $handler = shift;
	printf $handler "\n#endif // end of __GPIOAUGOGEN_H__\n";
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
				if($val eq "000008") {
				# $buf .= sprintf "    { %-40s, 0x%05X, 0x%05X },    \\\n", $thisPin->{srcNET_NAME}, hex $val, hex $msk;
		 $buf .= sprintf("    {%-20s%-45s%-27s},    \\\n", $thisPin->{srcNET_NAME}.", ", $thisPin->{VAL_STR}.$thisPin->{DIR_STR}.$thisPin->{INT_TYPE_STR}.$thisPin->{INTERNAL_PAD_STR}.", ","GPIO_CTRL_PWRG_OFF");
				} else {
		 $buf .= sprintf("    {%-20s%-45s%-27s},    \\\n", $thisPin->{srcNET_NAME}.", ", $thisPin->{VAL_STR}.$thisPin->{DIR_STR}.$thisPin->{INT_TYPE_STR}.$thisPin->{INTERNAL_PAD_STR}.", ","GPIO_CTRL_PWRG_VTR_IO");
				}
			} else {
				print "[ERROR] Incorrect RUNTIME STATUS $st ($thisPin->{$st}) of $thisPin->{PIN_NAME}. Skipped ...\n";
			}
		}
	}
	# $buf .= "    { GPIO_PIN_NULL                           , 0x00000, 0x00000 }";
	$buf = sprintf "#define GPIO_RUNTIME_SWITCH_%-5s                                      \\\n". $buf . "\n\n", $st;
	return $buf;
}

sub autoGenHeaderFile()
{
	my $handler = shift;

	# my $pinNameDbgString = "";

	my $fileBuf = "";

	$fileBuf .= "\n";
	$fileBuf .= "/***********************************************************************\n";
	$fileBuf .= " *                                                                     *\n";
	$fileBuf .= " * PIN CTRL INDEX definition sort by category                          *\n";
	$fileBuf .= " *                                                                     *\n";
	$fileBuf .= " ***********************************************************************/\n\n";
	my $curCategory = "xxxxx";
	my $categoryName = "";
	my $categoryHeader = "";
	my $porInitString = "";
	my $isNewCategory = 0;
	my @sortArr = ();
	foreach my $key (sort { $PIN_DEF{$a}->{CATEGORY} cmp $PIN_DEF{$b}->{CATEGORY} } keys %PIN_DEF) {
		if ($curCategory ne $PIN_DEF{$key}->{CATEGORY}) {
			$curCategory = $PIN_DEF{$key}->{CATEGORY};
			$isNewCategory = 1;
		}
		
		my $selfun = $PIN_DEF{$key}->{SELECTD_FUN};
		my $pinName = $PIN_DEF{$key}->{PIN_NAME};
		
		if ($isNewCategory) {
			if ( $#sortArr >= 0 ) {
				$fileBuf .= $categoryHeader;
				$categoryHeader = "";
				my $itemInLine = 0;
				foreach my $line (sort {$a cmp $b} @sortArr) {
					$fileBuf .= $line;
					if ($line =~ /#define\s*([^\s]+)/) {
						$porInitString .= "\\\n    " if ($itemInLine % 4 == 0 && $itemInLine != 0);
						$porInitString .= sprintf "%-30s", $1.",";
						$itemInLine ++;
					}
				}
				$fileBuf .= sprintf "\n#define POR_INIT_%-12s    \\\n    ", $categoryName;
				push @PorInitCategorys, "POR_INIT_".$categoryName;
				$porInitString =~ s/,\s*$//;
				$fileBuf .= $porInitString."\n\n";
				#
				# list KSI<x>/KSO<x> if it's KB
				#
				if ($categoryName =~ /Keyboard/i) {
					$porInitString =~ s/[\n\\\s]//g;
					my @tmpArr = split /,/, $porInitString;
					my @ksiArr = grep /KSI/i, @tmpArr;
					my @ksoArr = grep /KSO/i, @tmpArr;

					if (scalar @ksiArr > 0) {
						$itemInLine = 0;
						$fileBuf .= sprintf "\n#define KEYBOARD_KSI_LIST    \\\n    ";
						foreach my $item (sort {
																($a =~ /(\d+)/)[0] <=> ($b =~ /(\d+)/)[0]
																||
																$a cmp $b
															} @ksiArr ) {
							$fileBuf .= "\\\n    " if ($itemInLine % 4 == 0 && $itemInLine != 0);
							$fileBuf .= sprintf "%-30s", $item.",";
							$itemInLine ++;
						}
						$fileBuf =~ s/,\s*$//;
						$fileBuf .= "\n";
					}
					if (scalar @ksoArr > 0) {
						$itemInLine = 0;
						$fileBuf .= sprintf "\n#define KEYBOARD_KSO_LIST    \\\n    ";
						foreach my $item (sort {
																($a =~ /(\d+)/)[0] <=> ($b =~ /(\d+)/)[0]
																||
																$a cmp $b
															} @ksoArr) {
							$fileBuf .= "\\\n    " if ($itemInLine % 4 == 0 && $itemInLine != 0);
							$fileBuf .= sprintf "%-30s", $item.",";
							$itemInLine ++;
						}
						$fileBuf =~ s/,\s*$//;
						$fileBuf .= "\n\n";
					}
				}
				$porInitString = "";
				@sortArr = ();
			}
			
			$categoryHeader .= "/*******************\n";
			if ($curCategory eq "") {
				$categoryHeader .= " * MISC \n";
				$categoryName = "MISC";
			} else {
				$categoryHeader .= " * $curCategory \n";
				$categoryName = $curCategory;
			}
			$categoryHeader .= " *******************/\n";
			$isNewCategory = 0;
		}
		
		if (exists $PIN_DEF{$key}->{PIN_OPT_MACRO}) {
			push @sortArr, $PIN_DEF{$key}->{PIN_OPT_MACRO}." // ".sprintf("%-11s ", $selfun)." | ".sprintf("%-8s", $PIN_DEF{$key}->{SLOT_NAME}.".".$PIN_DEF{$key}->{SLOT_IDX})." | ".sprintf("%-10s", $PIN_DEF{$key}->{MDU_PIN_NAME})." | ".$PIN_DEF{$key}->{DESCRIPTION}."\n";
		}
	}
	if ( $#sortArr > 0 ) {
		$fileBuf .= $categoryHeader;
		$categoryHeader = "";
		my $itemInLine = 0;
		foreach my $line (sort {$a cmp $b} @sortArr) {
			$fileBuf .= $line;
			if ($line =~ /#define\s*([^\s]+)/) {
				$porInitString .= "\\\n    " if ($itemInLine % 4 == 0 && $itemInLine != 0);
				$porInitString .= sprintf "%-30s", $1.",";
				$itemInLine ++;
			}
		}
		$fileBuf .= sprintf "\n#define POR_INIT_%-12s    \\\n    ", $categoryName;
		push @PorInitCategorys, "POR_INIT_".$categoryName;
		$porInitString =~ s/,\s*$//;
		$fileBuf .= $porInitString."\n\n";
		$porInitString = "";
		@sortArr = ();
	}

	$fileBuf .= "/* Runtime GPIO settings */\n";
	# MACROS for RUNTIME SETTINGS
	# G3, S5/S4, S3, S0
	$fileBuf .= &_genRuntimeSwitchMacros('G3');
	$fileBuf .= &_genRuntimeSwitchMacros('S5S4');
	$fileBuf .= &_genRuntimeSwitchMacros('S3');
	$fileBuf .= &_genRuntimeSwitchMacros('S0');
	$fileBuf .= "
/**
 * \@brief change GPIO group status based on power sequence
 *
 * \@param current power sequence status
 * \@retval void
 */
void __autoGen_runtimeGpioSwitching (enum system_power_state pwr_state);

/**
 * \@brief Initialize EC GPIO
 *
 * \@retval 0 if successful
 */
int __autoGen_initECGPIO (void );\n\n";
	
# 	$fileBuf .= "/***********************************************************************\n";
# 	$fileBuf .= " *                                                                     *\n";
# 	$fileBuf .= " * Forward declaration AutoGen functions                               *\n";
# 	$fileBuf .= " *                                                                     *\n";
# 	$fileBuf .= " ***********************************************************************/";
# 	$fileBuf .= "
# extern void __autogen_GirqInit (void);
# extern void __autogen_GpioPorInit (void);
# extern void __autogen_GpioReset (void);
# extern void __autogen_GpioOverride(uint32_t * pList);";
# 	$fileBuf .= "\n\n";
	
# 	# EC ACPI Space
# 	$fileBuf .= "/***********************************************************************\n";
# 	$fileBuf .= " *                                                                     *\n";
# 	$fileBuf .= " * Forward declaration for EC ACPI space handlers                      *\n";
# 	$fileBuf .= " *                                                                     *\n";
# 	$fileBuf .= " ***********************************************************************/\n";
# 	foreach my $grp (sort {$a <=> $b} keys %EC_ACPI_GROUP) {
# 		if (scalar keys %{ $EC_ACPI_GROUP{$grp} }) {
# 			$fileBuf .= "extern void __autogen_AcpiHandler_GIO$grp (uint8_t isRead, uint8_t ui8Idx, uint8_t * pui8Data);\n";
# 		}
# 	}
	$fileBuf .= "
/***********************************************************************
 *                                                                     *
 * Forward declaration for EC ACPI space handlers                      *
 *                                                                     *
 ***********************************************************************/
extern void Board_Gpio_AcpiHandler (uint8_t isRead, uint8_t ui8Idx, uint8_t * pui8Data);
 ";

	printf $handler $fileBuf;
}

sub autoGenSourceFile
{
	my $handler = shift;
	
	my $fileBuf = "";
	my $ecNameBuf = "";

	# $ecNameBuf .= "\n\n";
	# $ecNameBuf .= "#if (__EC_NAME_ASL__)\n";

	$fileBuf .= "\n";
	$fileBuf .= "#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <errno.h>
#ifdef CONFIG_PINCTRL
#include <zephyr/drivers/pinctrl.h>
#else
#include <zephyr/drivers/pinmux.h>
#endif
#include \"system.h\"
#include \"app_pseq.h\"
#include <zephyr/logging/log.h>
#include \"gpio_ec.h\"
#include \"npcx4mnx_pin.h\"
#include \"board_config.h\"
#include \"app_acpi.h\"
#include \"gpioAutoGen.h\"
";
	
# 	# NET NAME string for logging
# 	$fileBuf .= "/***********************************************************************\n";
# 	$fileBuf .= " *                                                                     *\n";
# 	$fileBuf .= " * NET NAMEs for debug log                                             *\n";
# 	$fileBuf .= " * _dbg_NET_NAME(pin) returns pointer of NET NAME string.              *\n";
# 	$fileBuf .= " *                                                                     *\n";
# 	$fileBuf .= " ***********************************************************************/\n";
# 	$fileBuf .= "
# #if (LOG_PIN_NAME && LOG_ENABLE)
# char * g_dbgNetName[] = {
#     GPIO_NET_NAME_STRING,
# };
# #endif";
# 	$fileBuf .= "\n\n";
	
# 	# GIRQ init
# 	$fileBuf .= "/***********************************************************************\n";
# 	$fileBuf .= " *                                                                     *\n";
# 	$fileBuf .= " * GIRQ initialization                                                 *\n";
# 	$fileBuf .= " *                                                                     *\n";
# 	$fileBuf .= " ***********************************************************************/\n";
# 	$fileBuf .= "
# static struct {
# 	uint8_t girqGrp;
# 	uint8_t girqBit;
# } _s_girqInitList[] = {
# 	GIRQ_INIT_LIST,
# };

# void __autogen_GirqInit () {
#     for (uint8_t i = 0; _isGIRQ_IDX(_s_girqInitList[i].girqGrp); i++) {
#         GIRQ_Enable(_GIRQ_IDX(_s_girqInitList[i].girqGrp), _s_girqInitList[i].girqBit);
#     }
# }";
# 	$fileBuf .= "\n\n";

# 	# Power-On-Reset initialization
# 	$fileBuf .= "/***********************************************************************\n";
# 	$fileBuf .= " *                                                                     *\n";
# 	$fileBuf .= " * POR initialization                                                  *\n";
# 	$fileBuf .= " *                                                                     *\n";
# 	$fileBuf .= " ***********************************************************************/\n";
# 	$fileBuf .= "\n";
# 	$fileBuf .= "uint32_t _s_autogen_PorInitTable[] = {\n";
# 	foreach my $c (@PorInitCategorys) {
# 		$fileBuf .= "    $c,\n";
# 	}
# 	$fileBuf .= "\n";
# 	$fileBuf .= "    GPIO_PIN_NULL\n";
# 	$fileBuf .= "};\n";
# 	$fileBuf .= "
# void __autogen_GpioPorInit () {
#     for (uint8_t i = 0; !GPIO_isNullPin( _s_autogen_PorInitTable[i] ); i++) {
#         GPIO_POR_Init( _s_autogen_PorInitTable[i] );
#     }
# }

# void __autogen_GpioReset () {
#     for (uint8_t i = 0; !GPIO_isNullPin( _s_autogen_PorInitTable[i] ); i++) {
#         GPIO_Reset( _s_autogen_PorInitTable[i] );
#     }
# }

# void __autogen_GpioOverride(uint32_t * pList) {
#     if (NULL == pList)
#         return;

#     for ( ; !GPIO_isNullPin( *pList ); pList++) {
#         for (uint8_t i = 0; !GPIO_isNullPin( _s_autogen_PorInitTable[i] ); i++) {
#             if ( PID(*pList) == PID(_s_autogen_PorInitTable[i]) ) {
#                 uint32_t s = PSN(_s_autogen_PorInitTable[i]);

#                 // Update pin defineation but keep SN as original.
#                 _s_autogen_PorInitTable[i] = (~(_SN(0))) & (*pList);
#                 _s_autogen_PorInitTable[i] |= _SN(s);

#                 // apply GPIO settings
#                 GPIO_POR_Init( _s_autogen_PorInitTable[i] );
#             }
#         }
#     }
# }";

	$fileBuf .= "\n";
	$fileBuf .= "
/** \@brief EC FW app owned gpios list.
 *
 * This list is not exhaustive, it do not include driver-owned pins,
 * the initialization is done as part of corresponding Zephyr pinmux driver.
 * BSP drivers are responsible to control gpios in soc power transitions and
 * system transitions.
 *
 * Note: Pins not assigned to any app function are identified with their
 * original pin number instead of signal
 *
 */\n";
	$fileBuf .= "
/* APP-owned gpios */\n";
	my %hashBuf = ();
	foreach my $key (sort { $PIN_DEF{$a}->{PIN_NAME} cmp $PIN_DEF{$b}->{PIN_NAME} } keys %PIN_DEF) {
		my $thisPin = $PIN_DEF{$key};
		if ($thisPin->{PIN_NAME} =~ /GPIO(([A-Fa-f0-9+-]+){1})(\d)/) {
			my $grpIdx = $1;
			my $pinIdx = $3;
			printf("$thisPin->{PIN_NAME} grp$grpIdx pin$pinIdx\n");
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

	# generate GPIO Ctrls
	$fileBuf .= "struct gpio_ec_config AMDNPCX4_CFG[] =  {";
	foreach my $grp (sort {$a cmp $b} keys %hashBuf) {
		$fileBuf .= "\n";
		$fileBuf .= "/***************************************************************************************************\n";
		$fileBuf .= " * FUN/PAD                                                                                                            default     \n";
		foreach my $idx (sort {$a cmp $b} keys %{ $hashBuf{$grp} } ) {
			$fileBuf .= $hashBuf{$grp}->{$idx}->{COMMENT}."\n";
		}
		$fileBuf .= " ***************************************************************************************************/\n";
		
		my $dbgBuf = "";
		my $isUartDbgFlag = 0xFF;
		my $isTRACEDbgFlag = 0;
		foreach my $idx (sort {$a cmp $b} keys %{ $hashBuf{$grp} } ) {
			my $thisPin = $hashBuf{$grp}->{$idx}->{THE_PIN};
						print "grp is  $grp idx is$idx  name $thisPin->{NET_NAME} num $thisPin->{PIN_SERIAL_NUM}\n";
			if ( exists $thisPin->{PIN_CTRL_MACRO_DBG_UART} && $thisPin->{PIN_NAME} =~ /UART([01])_[TR]X/) {
				my $uartPort = $1;
				if ($isTRACEDbgFlag) {
					$fileBuf .= $dbgBuf;
					$dbgBuf = "";
					$fileBuf .= "#endif\n";
					$isTRACEDbgFlag = 0;
				}
				if ($isUartDbgFlag != $uartPort) {
					$isUartDbgFlag = $uartPort;
					if ($uartPort == 0) {
						$fileBuf .= "#if ((DEBUG_UART_PORT == 0 && DEBUG_UART_TRACE) || DEBUG_UART0_AT_COM2)\n";
					} else {
						$fileBuf .= "#if (DEBUG_UART_PORT == $uartPort && DEBUG_UART_TRACE)\n";
					}
					$dbgBuf .= "#else\n"
				}
				$fileBuf .= $thisPin->{PIN_CTRL_MACRO_DBG_UART}."\n";
				$dbgBuf .= $hashBuf{$grp}->{$idx}->{DEFINE} ."\n";
			} elsif ( exists $thisPin->{PIN_CTRL_MACRO_DBG_TRACE} && $thisPin->{PIN_NAME} =~ /TRACEDAT[01234]/) {
				if ($isUartDbgFlag != 0xFF) {
					$fileBuf .= $dbgBuf;
					$dbgBuf = "";
					$fileBuf .= "#endif\n";
					$isUartDbgFlag = 0xFF;
				}
				if ($isTRACEDbgFlag == 0) {
					$isTRACEDbgFlag = 1;
					$fileBuf .= "#if (DEBUG_ETM_TRACE)\n";
					$dbgBuf .= "#else\n"
				}
				$fileBuf .= $thisPin->{PIN_CTRL_MACRO_DBG_TRACE}."\n";
				$dbgBuf .= $hashBuf{$grp}->{$idx}->{DEFINE}."\n";
			} else {
				if ($isTRACEDbgFlag) {
					$fileBuf .= $dbgBuf;
					$dbgBuf = "";
					$fileBuf .= "#endif\n";
					$isTRACEDbgFlag = 0;
				}
				if ($isUartDbgFlag != 0xFF) {
					$fileBuf .= $dbgBuf;
					$dbgBuf = "";
					$fileBuf .= "#endif\n";
					$isUartDbgFlag = 0xFF;
				}
				$fileBuf .= $hashBuf{$grp}->{$idx}->{DEFINE}."\n";
			}
			# $pinNameDbgString .= sprintf "    %-40s   /* %3d */   \\\n", "\"".$thisPin->{NET_NAME}."\",", $thisPin->{PIN_SERIAL_NUM};
		}
		if ($dbgBuf ne "") {
			$fileBuf .= $dbgBuf;
			$fileBuf .= "#endif\n";
		}
		$fileBuf .= "\n";
	}
	$fileBuf .= "};\n";
	$fileBuf .= "
int __autoGen_initECGPIO (void )
{
    int ret = 0;
	ret = gpio_configure_array(AMDNPCX4_CFG, ARRAY_SIZE(AMDNPCX4_CFG));
    return ret;
}\n";
	# RUNTIME SWITCH handler
	$fileBuf .= "/***********************************************************************\n";
	$fileBuf .= " *                                                                     *\n";
	$fileBuf .= " * Runtime GPIO settings                                               *\n";
	$fileBuf .= " *                                                                     *\n";
	$fileBuf .= " ***********************************************************************/\n";
	$fileBuf .= "
struct gpio_ec_config _s_AMDNPCX4_cfg_rtG3[] = {
    GPIO_RUNTIME_SWITCH_G3
};
struct gpio_ec_config _s_AMDNPCX4_cfg_rtS5S4[] = {
    GPIO_RUNTIME_SWITCH_S5S4
};
struct gpio_ec_config _s_AMDNPCX4_cfg_rtS3[] = {
    GPIO_RUNTIME_SWITCH_S3
};
struct gpio_ec_config _s_AMDNPCX4_cfg_rtS0[] = {
    GPIO_RUNTIME_SWITCH_S0
};

void __autoGen_runtimeGpioSwitching (enum system_power_state pwr_state ) {
    switch (pwr_state) {
        case SYSTEM_G3_STATE:   
			gpio_configure_array(_s_AMDNPCX4_cfg_rtG3, ARRAY_SIZE(_s_AMDNPCX4_cfg_rtG3));   
			break;
        case SYSTEM_S4_STATE:
        case SYSTEM_S5_STATE:
			gpio_configure_array(_s_AMDNPCX4_cfg_rtS5S4, ARRAY_SIZE(_s_AMDNPCX4_cfg_rtS5S4)); 
			break;
        case SYSTEM_S3_STATE:   
			gpio_configure_array(_s_AMDNPCX4_cfg_rtS3, ARRAY_SIZE(_s_AMDNPCX4_cfg_rtS3));   
			break;
        case SYSTEM_S0_STATE:
			gpio_configure_array(_s_AMDNPCX4_cfg_rtS0, ARRAY_SIZE(_s_AMDNPCX4_cfg_rtS0));
			break;  
        default:                                                   
			break;
    }
}";
	$fileBuf .= "\n\n";

	$fileBuf .= "/***********************************************************************\n";
	$fileBuf .= " *                                                                     *\n";
	$fileBuf .= " * EC ACPI Space                                                       *\n";
	$fileBuf .= " *                                                                     *\n";
	$fileBuf .= " ***********************************************************************/\n";
	$fileBuf .= "\n";
	foreach my $grp (sort {$a <=> $b} keys %EC_ACPI_GROUP) {
		my $source = "";
		my $readSrc = "";
		my $writeSrc = "";
		my $flag = 0;

		$source .= "void __autogen_AcpiHandler_GIO$grp (uint8_t isRead, uint8_t ui8Idx, uint8_t * pui8Data)\n"; 
		$source .= "{\n";
		$source .= "    uint8_t retVal = 0;";
		$source .= "\n";
		$source .= sprintf "    if (ui8Idx != ACPI_GPIO_BASE + %d) return;\n", $grp + 3;
		$source .= "\n";
		$source .= "    if (isRead) {\n";
		$source .= "__READ_PLACE_HOLDER__";
		$source .= "    } else if (!pui8Data){\n";
		$source .= "        return;\n";
		$source .= "    } else {\n";
		$source .= "__WRITE_PLACE_HOLDER__";
		$source .= "    }\n";
		$source .= "}\n";
		$writeSrc .= "        retVal = *pui8Data;\n\n";
		
		my $lastPrintBit = -1;

		foreach my $bit (sort {$a <=> $b} keys %{ $EC_ACPI_GROUP{$grp} } ) {
			my $thisPin = $EC_ACPI_GROUP{$grp}->{$bit};

			# if (!$flag) {
			# 	$ecNameBuf.= "__autogen_AcpiHandler_GIO$grp ()\n";
			# }

			$flag = 1;
			$readSrc  .= sprintf("        retVal |= gpio_read_pin( %-22s ? 0x%02X : 0x00;	// %s\n", $thisPin->{srcNET_NAME}." )", 0x01 << $bit, $thisPin->{PIN_NAME});
			$writeSrc .= sprintf("        gpio_write_pin( %-22s(retVal & 0x%02X) ? 1 : 0 );	// %s\n", $thisPin->{srcNET_NAME}." ,", 0x01 << $bit, $thisPin->{PIN_NAME});

			# if ($bit > $lastPrintBit + 2) {
			# 	$ecNameBuf.= sprintf("                  // Bit[%d:%d] - Reserved\n", $lastPrintBit + 1, $bit - 1);
			# } elsif ($bit == $lastPrintBit + 2) {
			# 	$ecNameBuf.= sprintf("                  // Bit[%d]   - Reserved\n", $bit - 1);
			# }

			$lastPrintBit = $bit;
			# $ecNameBuf.= sprintf("                  // Bit[%d]   - %-22s  | %-5s | %s | %s\n", $bit, $thisPin->{srcNET_NAME}, $thisPin->{SLOT_NAME} . "." . $thisPin->{SLOT_IDX}, $thisPin->{SETTING_BRIEF}, $thisPin->{DESCRIPTION});
		}
		if ($flag) {
			$readSrc .= "\n        if (pui8Data) *pui8Data = retVal;\n";
	
			$source =~ s/__READ_PLACE_HOLDER__/$readSrc/;
			$source =~ s/__WRITE_PLACE_HOLDER__/$writeSrc/;

			# if ($lastPrintBit == 6) {
			# 	$ecNameBuf.= sprintf("                  // Bit[%d]   - Reserved\n", $lastPrintBit + 1);
			# } elsif ($lastPrintBit < 6) {
			# 	$ecNameBuf.= sprintf("                  // Bit[%d:%d] - Reserved\n", $lastPrintBit + 1, 7);
			# }

			$fileBuf .= $source."\n";
			# $ecNameBuf.= "\n";
		}
	}
	$fileBuf .= "
void Board_Gpio_AcpiHandler (uint8_t isRead, uint8_t ui8Idx, uint8_t * pui8Data)
{
	switch (ui8Idx)
	{
		case ACPI_GPIO_BASE:

			break;
		case ACPI_GPIO_BASE+1:

			break;
		case ACPI_GPIO_BASE+2:

			break;
		case ACPI_GPIO_BASE+3:
			__autogen_AcpiHandler_GIO0(isRead,ui8Idx,pui8Data);
			break;
		case ACPI_GPIO_BASE+4:
			__autogen_AcpiHandler_GIO1(isRead,ui8Idx,pui8Data);
			break;
		case ACPI_GPIO_BASE+5:
			__autogen_AcpiHandler_GIO2(isRead,ui8Idx,pui8Data);
			break;
		case ACPI_GPIO_BASE+6:
 			__autogen_AcpiHandler_GIO3(isRead,ui8Idx,pui8Data);
			break;
		case ACPI_GPIO_BASE+7:
			__autogen_AcpiHandler_GIO4(isRead,ui8Idx,pui8Data);
			break;
		case ACPI_GPIO_BASE+8:
			board_ioexp_AcpiHandler_IOExp10(isRead,ui8Idx,pui8Data);
			break;
		case ACPI_GPIO_BASE+9:
			board_ioexp_AcpiHandler_IOExp11(isRead,ui8Idx,pui8Data);
			break;
		case ACPI_GPIO_BASE+10:
			board_ioexp_AcpiHandler_IOExp0(isRead,ui8Idx,pui8Data);
			break;
		case ACPI_GPIO_BASE+11:
			board_ioexp_AcpiHandler_IOExp1(isRead,ui8Idx,pui8Data);
			break;
		case ACPI_GPIO_BASE+12:
			board_ioexp_AcpiHandler_IOExp2(isRead,ui8Idx,pui8Data);
			break;
		case ACPI_GPIO_BASE+13:
			board_ioexp_AcpiHandler_IOExp3(isRead,ui8Idx,pui8Data);
			break;
		case ACPI_GPIO_BASE+14:
			board_ioexp_AcpiHandler_IOExp4(isRead,ui8Idx,pui8Data);
			break;
		case ACPI_GPIO_BASE+15:
			board_ioexp_AcpiHandler_IOExp5(isRead,ui8Idx,pui8Data);
			break;
		case ACPI_GPIO_BASE+16:
			board_ioexp_AcpiHandler_IOExp6(isRead,ui8Idx,pui8Data);
			break;
		case ACPI_GPIO_BASE+17:
			board_ioexp_AcpiHandler_IOExp7(isRead,ui8Idx,pui8Data);
			break;
		case ACPI_GPIO_BASE+18:
			board_ioexp_AcpiHandler_IOExp8(isRead,ui8Idx,pui8Data);
			break;
		case ACPI_GPIO_BASE+19:
			board_ioexp_AcpiHandler_IOExp9(isRead,ui8Idx,pui8Data);
			break;
		}

}";
	printf $handler $fileBuf;

	# #
	# # EC NAME of IO Expander
	# #
	# foreach my $key (sort keys %IO_EXP_INIT_REG) {
	# 	my $maskTable = $IO_EXP_INIT_REG{$key};

	# 	# $ecNameBuf .= $maskTable->{ECNAME_INFO};
	# 	# $ecNameBuf .= "\n";
	# }

	# $ecNameBuf .= "#endif\n";
	# printf $handler $ecNameBuf;
}


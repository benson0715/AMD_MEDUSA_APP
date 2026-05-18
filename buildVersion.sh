#!/bin/bash
# Script to build the version string

PROJECT=$1

echo "****************************"
echo "* Prebuild for $PROJECT"
echo "****************************"

BUILD_DATE=`date +%Y-%m-%d`
BUILD_TIME=`date +%H%M%S`

cd ../ > /dev/null
HEAD_VERSION=`git log --pretty=oneline --abbrev-commit | head -1 | awk '{print $1}' | cut -c 1-7`
COMMON_CHANGES=`git status -suno | grep -v 'ec_version.h$' | grep -v '\sProjects/'${PROJECT}`
PROJECT_CHANGES=`git status -suno | grep -v 'ec_version.h$' | grep Projects/${PROJECT}`

if [ T"${COMMON_CHANGES}" != T"" -o T"${PROJECT_CHANGES}" != T"" ]; then
	echo "***WARNING****"
	echo "There are local changes:"
	echo
	if [ T"$COMMON_CHANGES" != T"" ] ; then
		echo "$COMMON_CHANGES"
	fi
	if [ T"$PROJECT_CHANGES" != T"" ] ; then
		echo "$PROJECT_CHANGES"
	fi
	echo 
	HEAD_VERSION=$HEAD_VERSION"*"
else
	HEAD_VERSION=$HEAD_VERSION"."
fi


cd ecfw-zephyr/boards/$1/source > /dev/null

cat ec_version.h | awk '
	{
		if ($0 ~ /#define EC_VERSION_STRING_2_VALUE/) {
			printf ("#define EC_VERSION_STRING_2_VALUE     \"%s\"         /* 0x2014: 10-bytes */\n", "'$BUILD_DATE'");
		} else if ($0 ~ /#define EC_VERSION_STRING_5_VALUE/) {
			printf ("#define EC_VERSION_STRING_5_VALUE     \"%s\"           /* 0x2028:  Repository Version; Local changes flag(*|.) */\n", "'$HEAD_VERSION'");
		} else if ($0 ~ /#define EC_VERSION_STRING_6_VALUE/) {
			printf ("#define EC_VERSION_STRING_6_VALUE     \"%s\"             /*          Build time */\n", "'$BUILD_TIME'");
		} else {
			print $0;
		}
	}
' > version_tmpx.txt
SHA1SUM=`sha1sum version_tmpx.txt | cut -c 1-40`
SHA1UINT32=`echo $SHA1SUM | awk '{ 
  XxX = strtonum(sprintf ("0x%s", substr($0, 1, 8))) + strtonum(sprintf ("0x%s", substr($0, 9, 8))) + strtonum(sprintf ("0x%s", substr($0, 17, 8))) + strtonum(sprintf ("0x%s", substr($0, 25, 8))) + strtonum(sprintf ("0x%s", substr($0, 33, 8)));
  XxX = and (XxX, 0xFFFFFFFF);
  printf "0x%08X", XxX; }'`
cat version_tmpx.txt | awk '
	{
		if ($0 ~ /#define EC_VERSION_STRING_7_VALUE/) {
			printf ("#define EC_VERSION_STRING_7_VALUE     \"%s\" /* VERSION SHA1SUM */\n", "'$SHA1SUM'");
		} else if ($0 ~ /#define EC_VERSION_U32_MAGIC/) {
			printf ("#define EC_VERSION_U32_MAGIC          0x%08Xul         /* uint32_t magic of SHA1SUM */\n", '$SHA1UINT32');
		} else {
			print $0;
		}
	}
' > version_tmp.txt
dos2unix version_tmp.txt > /dev/null 2>&1
rm -f version_tmpx.txt
mv version_tmp.txt ec_version.h

echo "Patch ec_version.h as below:"
git diff -U1 ./ec_version.h

cd ../../../../

echo "****************************"
echo "* Prebuild done ..."
echo "****************************"

# Copyright (C) 2023 Advanced Micro Devices, Inc. All rights reserved.

SECTION_PROLOGUE(_CUSTOM_APP_SECTION_NAME,,ALIGN(32))
{
	KEEP(*(".start_info.*"));
	KEEP(*(".ecfw_info.*"));
} GROUP_LINK_IN(AMDECVER)
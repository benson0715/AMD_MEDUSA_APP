.. _coding_style:

Coding Style
############

Overview
********

Use these coding guidelines to ensure that your development complies with the project's style and naming
conventions.
In general, follow the `Linux kernel coding style`_. Some coding rules follow AMD EC team.

.. _Linux kernel coding style: https://kernel.org/doc/html/latest/process/coding-style.html

Example
*******

**Indentation**

Use a single tab for each indentation (8 characters).

.. code-block:: c

	if (mflags & I2C_MSG_STOP) {
		ret = do_stop(dev, STOP_WAIT_COUNT);
	}

**Always use { }**

.. code-block:: c

	if (x is true) {
	  	we do y
	}

.. code-block:: c

	switch (action) {
	case KOBJ_ADD:
		return "add";
	case KOBJ_REMOVE:
		return "remove";
	case KOBJ_CHANGE:
		return "change";
	default:
		return NULL;
	}

**CamelCase (AMD)**

Used for variables, functions and file names 

Correct:

.. code-block:: c

	void SetSystemState(OSPM_Type newstate)
	{

	}

Incorrect:

.. code-block:: c

	void setsystemstate(OSPM_Type newstate)
	{

	}

**Copyright Statements (AMD)**

Follow `Copyright Statements of Client BIOS process`_.
For 2022, the Copyright and License text has been reduced to a single line Copyright statement

.. _Copyright Statements of Client BIOS process: https://confluence.amd.com/pages/viewpage.action?spaceKey=BIOSC&title=Copyright+Statements

For new files

.. code-block:: c

   /*****************************************************************************
   *
   * Copyright (C) 2023 Advanced Micro Devices, Inc. All rights reserved.
   *
   ******************************************************************************
   */

For pre-existing file that is modified
Include the first year in which the software was first developed and the last year it was substantively modified:

.. code-block:: c

   /******************************************************************************
   Copyright (C) [insert range of years] Advanced Micro Devices, Inc. All rights reserved.
   *****************************************************************************/

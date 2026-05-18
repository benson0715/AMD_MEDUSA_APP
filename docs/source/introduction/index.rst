.. _introducing_AMD_zephyr:

Document Overview
#################

In this document, it describes the Zephyr EC development environment, 
coding convention, coding style and how to utilize different Zephyr module API 
(GPIO, I2C, eSPI, etc.) to do EC application on AMD CRB.

Document Scope
**************

Embedded Controller (EC) is an important component of the mobile PC platform. 
It usually handles Power Sequence, Battery Charging, Thermal Control, etc.
This document presents the Zephyr RTOS based EC firmware development. 
Zephyr is a Linux Foundation hosted Collaboration Project since 2016. It delivers the 
best-in-class RTOS for connected resource-constrained devices and supports more than 200 board 
including multiple hardware architectures, ARM, x86, RISCV, etc. 
The Zephyr project has Long Term Support (LTS) version.
To ease and accelerate customer’s firmware development, the Zephyr RTOS base Embedded Controller is a good choice. 
OEMs/ODMs can leverage the open-source EC FW code to use as a base and customize for the system requirements.

Licensing
*********

Zephyr is permissively licensed using the Apache 2.0 license
There are some imported or reused components of the Zephyr project that use other licensing,

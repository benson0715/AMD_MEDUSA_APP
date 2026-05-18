/*****************************************************************************
 * Copyright (C) 2023 Advanced Micro Devices, Inc. All rights reserved.
 ******************************************************************************
 */

#include "acpiplanes.h"

LOG_MODULE_REGISTER(acpiplanes, CONFIG_ACPIPLANES_LOG_LEVEL);

struct {
	const MD_ACPIPLANES_HANDLER_TABLE * pTab;
	uint8_t ui8tabSize;
	MD_ACPI_HYPERPLANE_LAUNCHER pFun;
	uint8_t planeId;
} _s_xAcpiPlanes [MD_ACPI_HYPERPLANE_MAX_PLANES];

static uint8_t _gs_acpiPlanesId = MD_ACPI_HYPERPLANE_FIXED_PLANE_ID;
uint8_t ui8AcpiSelectorUpdate = 0;

/* Always points to the active planes */
static const MD_ACPIPLANES_HANDLER_TABLE * _gs_pCurTab = NULL;
static uint8_t _gs_tabSize = 0;
static MD_ACPI_HYPERPLANE_LAUNCHER _gs_pCurFun = NULL;

/* The default plane for fixed region */
/* The fixed plane has fixed plane ID (MD_ACPI_HYPERPLANE_FIXED_PLANE_ID) */
static const MD_ACPIPLANES_HANDLER_TABLE * _gs_pFixedPlane = NULL;
static uint8_t _gs_fixedPlaneTabSize = 0;
static MD_ACPI_HYPERPLANE_LAUNCHER _gs_pFixedFun = NULL;

/**
 * @brief Initialize the ACPI planes module
 *
 * Clears all plane registrations and resets internal state variables.
 */
void md_acpiplanes_init()
{
	memset(&_s_xAcpiPlanes, 0, sizeof(_s_xAcpiPlanes));
	_gs_pCurTab = NULL;
	_gs_pCurFun = NULL;

	/* for fixed plane */
	_gs_pFixedPlane = NULL;
	_gs_fixedPlaneTabSize = 0;
	_gs_pFixedFun = NULL;
}

/**
 * @brief Register a handler table for an ACPI plane
 *
 * @param pTab Pointer to the handler table
 * @param ui8tabSize Size of the handler table
 * @param planeId Plane identifier to register
 * @return true if registration successful, false otherwise
 */
bool md_acpiplanes_register_tab(const MD_ACPIPLANES_HANDLER_TABLE * pTab, uint8_t ui8tabSize, uint8_t planeId)
{
	/* The fixed plane has fixed plane ID 0xFF */
	if (planeId == MD_ACPI_HYPERPLANE_FIXED_PLANE_ID) {
		_gs_pFixedFun = NULL;
		_gs_pFixedPlane = pTab;
		_gs_fixedPlaneTabSize = ui8tabSize;
		return true;
	}

	for (uint8_t i = 0; i < MD_ACPI_HYPERPLANE_MAX_PLANES; i++) {
		if (_s_xAcpiPlanes[i].planeId == 0 && _s_xAcpiPlanes[i].pTab == NULL && _s_xAcpiPlanes[i].pFun == NULL) {
			_s_xAcpiPlanes[i].planeId = planeId;
			_s_xAcpiPlanes[i].pTab = pTab;
			_s_xAcpiPlanes[i].ui8tabSize = ui8tabSize;
			_s_xAcpiPlanes[i].pFun = NULL;
			return true;
		}

		if (_s_xAcpiPlanes[i].planeId == planeId) {
			LOG_WRN("ACPI plane 0x%02X has allocated!", planeId);
		}
	}

	LOG_ERR("No more space for ACPI plane 0x%02X!", planeId);

	assert(0);
	return false;
}

/**
 * @brief Register a launcher function for an ACPI plane
 *
 * @param pFun Pointer to the launcher function
 * @param planeId Plane identifier to register
 * @return true if registration successful, false otherwise
 */
bool md_acpiplanes_register_fun(MD_ACPI_HYPERPLANE_LAUNCHER pFun, uint8_t planeId)
{
	/* The fixed plane has fixed plane ID 0xFF */
	if (planeId == MD_ACPI_HYPERPLANE_FIXED_PLANE_ID) {
		_gs_pFixedPlane = NULL;
		_gs_fixedPlaneTabSize = 0;
		_gs_pFixedFun = pFun;
		return true;
	}

	for (uint8_t i = 0; i < MD_ACPI_HYPERPLANE_MAX_PLANES; i++) {
		if (_s_xAcpiPlanes[i].planeId == 0 && _s_xAcpiPlanes[i].pTab == NULL && _s_xAcpiPlanes[i].pFun == NULL) {
			_s_xAcpiPlanes[i].planeId = planeId;
			_s_xAcpiPlanes[i].pTab = NULL;
			_s_xAcpiPlanes[i].pFun = pFun;
			return true;
		}

		if (_s_xAcpiPlanes[i].planeId == planeId) {
			LOG_WRN("ACPI plane 0x%02X has allocated!", planeId);
		}
	}

	LOG_ERR("No more space for ACPI plane 0x%02X!", planeId);

	assert(0);
	return false;
}

/**
 * @brief Deregister an ACPI plane
 *
 * @param planeId Plane identifier to deregister
 * @return true if deregistration successful, false if plane not found
 */
bool md_acpiplanes_deregister(uint8_t planeId)
{
	for (uint8_t i = 0; i < MD_ACPI_HYPERPLANE_MAX_PLANES; i++) {
		if (_s_xAcpiPlanes[i].planeId == planeId && (_s_xAcpiPlanes[i].pTab || _s_xAcpiPlanes[i].pFun)) {
			_s_xAcpiPlanes[i].planeId = 0;
			_s_xAcpiPlanes[i].pTab = NULL;
			_s_xAcpiPlanes[i].pFun = NULL;
			return true;
		}
	}

	return false;
}

/**
 * @brief Dispatch ACPI read/write operations to the appropriate plane handler
 *
 * @param isRead Non-zero for read operation, zero for write operation
 * @param ui8Idx Register index to access
 * @param pui8Data Pointer to data buffer for read/write
 * @return 1 if handled successfully, 0 otherwise
 */
uint8_t md_acpiplanes_dispatcher(uint8_t isRead, uint8_t ui8Idx, uint8_t * pui8Data)
{
	/* Handle the plane selector */
	if (ui8Idx == MD_ACPI_HYPERPLANE_SELECTOR_OFFSET) {
		if (isRead) {
			*pui8Data = _gs_acpiPlanesId;
		} else {
			uint8_t planeId = *pui8Data;
			if (planeId == MD_ACPI_HYPERPLANE_FIXED_PLANE_ID) {
				_gs_pCurFun = NULL;
				_gs_pCurTab = NULL;
				_gs_tabSize = 0;
				_gs_acpiPlanesId = planeId;
			} else {
				for (uint8_t i = 0; i < MD_ACPI_HYPERPLANE_MAX_PLANES; i++) {
					if (_s_xAcpiPlanes[i].planeId == planeId) {
						if (_s_xAcpiPlanes[i].pTab) {
							_gs_pCurFun = NULL;
							_gs_pCurTab = _s_xAcpiPlanes[i].pTab;
							_gs_tabSize = _s_xAcpiPlanes[i].ui8tabSize;
							_gs_acpiPlanesId = planeId;
						} else if (_s_xAcpiPlanes[i].pFun) {
							_gs_pCurTab = NULL;
							_gs_tabSize = 0;
							_gs_pCurFun = _s_xAcpiPlanes[i].pFun;
							_gs_acpiPlanesId = planeId;
						} else {
							/* do nothing if the plane is not registered. */
						}
					}
				}
			}
			ui8AcpiSelectorUpdate = *pui8Data;
		}
		return 1;
	}

	/* if idx is out of hyperplane region or 
	   within the hyperplane region but active plane is MD_ACPI_HYPERPLANE_FIXED_PLANE_ID */
	if ((ui8Idx < MD_ACPI_HYPERPLANE_REGION_START || ui8Idx > MD_ACPI_HYPERPLANE_REGION_END) || 
		_gs_acpiPlanesId == MD_ACPI_HYPERPLANE_FIXED_PLANE_ID ) {
		if (_gs_pFixedPlane && _gs_fixedPlaneTabSize) {
			uint8_t idx = 0;

			/* binary search */
			int32_t il, ih, im;
			il = 0;
			ih = _gs_fixedPlaneTabSize;
			while (il <= ih) {
				im = (il + ih) / 2;
				if (_gs_pFixedPlane[im].ui8Register > ui8Idx) {
					ih = im - 1;
				} else if (_gs_pFixedPlane[im].ui8Register < ui8Idx) {
					il = im + 1;
				} else {
					idx = im;
					if (idx < _gs_fixedPlaneTabSize && _gs_pFixedPlane[idx].pfnHandler != NULL && pui8Data != NULL) {
						_gs_pFixedPlane[idx].pfnHandler(isRead, ui8Idx, pui8Data);
						if (ui8Idx >= _LOG_EC_RAM_RANGE_START && ui8Idx <= _LOG_EC_RAM_RANGE_END) {
							LOG_DBG("%s [%02X:%02X] %02X", isRead ? "R" : "W", MD_ACPI_HYPERPLANE_FIXED_PLANE_ID, ui8Idx, *pui8Data);
						}
						return 1;
					}
					break;
				}
			}
		} else if (_gs_pFixedFun) {
			uint8_t ret = _gs_pFixedFun(isRead, ui8Idx, pui8Data);
			if (ui8Idx >= _LOG_EC_RAM_RANGE_START && ui8Idx <= _LOG_EC_RAM_RANGE_END) {
				LOG_DBG("%s [%02X:%02X] %02X", isRead ? "R" : "W", MD_ACPI_HYPERPLANE_FIXED_PLANE_ID, ui8Idx, *pui8Data);
			}
			return ret;
		}

		return 0;
	}

	/* handle the active planes */
	if (_gs_pCurTab && _gs_tabSize) {
		uint8_t idx = 0;

		/* binary search */
		int32_t il, ih, im;
		il = 0;
		ih = _gs_tabSize;
		while (il <= ih) {
			im = (il + ih) / 2;
			if (_gs_pCurTab[im].ui8Register > ui8Idx) {
				ih = im - 1;
			} else if (_gs_pCurTab[im].ui8Register < ui8Idx) {
				il = im + 1;
			} else {
				idx = im;
				if (idx < _gs_tabSize && _gs_pCurTab[idx].pfnHandler != NULL && pui8Data != NULL) {
					_gs_pCurTab[idx].pfnHandler(isRead, ui8Idx, pui8Data);
					if (ui8Idx >= _LOG_EC_RAM_RANGE_START && ui8Idx <= _LOG_EC_RAM_RANGE_END) {
						LOG_DBG("%s [%02X:%02X] %02X", isRead ? "R" : "W", _gs_acpiPlanesId, ui8Idx, *pui8Data);
					}
					return 1;
				}
				break;
			}
		}
	} else if (_gs_pCurFun) {
		uint8_t ret = _gs_pCurFun(isRead, ui8Idx, pui8Data);
		if (ui8Idx >= _LOG_EC_RAM_RANGE_START && ui8Idx <= _LOG_EC_RAM_RANGE_END) {
			LOG_DBG("%s [%02X:%02X] %02X", isRead ? "R" : "W", _gs_acpiPlanesId, ui8Idx, *pui8Data);
		}
		return ret;
	}

	return 0;
}
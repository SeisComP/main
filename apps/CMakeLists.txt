SUBDIRS(fdsnws processing python tools)

IF(SC_GLOBAL_GUI)
	SUBDIRS(gui-qt)
ENDIF()
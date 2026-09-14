set (QML_FILES
	Contact.qml
	ContactsGridView.qml
)

install (FILES ${QML_FILES}
	DESTINATION "${KADU_INSTALL_DATA_DIR}/qml"
)

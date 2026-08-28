set (message_SRCS
	message.cpp
	message-filter-service.cpp
	message-manager.cpp
	message-manager-impl.cpp
	message-module.cpp
	message-notification-service.cpp
	message-shared.cpp
	message-storage.cpp
	raw-message.cpp
	sorted-messages.cpp
	unread-message-repository.cpp
)

kadu_subdirectory (message "${message_SRCS}")

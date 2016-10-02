#include <iostream>
#include <windows.h>
#include <iphlpapi.h>

#include "eventlog.h"

bool GetIfTable(PMIB_IFTABLE* m_pTable);

void install_event_log_source(const std::string& a_name) {
  const std::string key_path(
      "SYSTEM\\CurrentControlSet\\Services\\"
      "EventLog\\Application\\" +
      a_name);

  HKEY key;

  DWORD last_error =
      RegCreateKeyEx(HKEY_LOCAL_MACHINE, key_path.c_str(), 0, 0,
                     REG_OPTION_NON_VOLATILE, KEY_SET_VALUE, 0, &key, 0);

  if (ERROR_SUCCESS == last_error) {

    UINT max_path = 512 * sizeof(WCHAR);
    HMODULE hModule = GetModuleHandleW(NULL);
    WCHAR exe_path[max_path];
    GetModuleFileNameW(hModule, exe_path, max_path);

    //BYTE exe_path[] = "C:\\path\\to\\your\\application.exe";
    DWORD last_error;
    const DWORD types_supported =
        EVENTLOG_ERROR_TYPE | EVENTLOG_WARNING_TYPE | EVENTLOG_INFORMATION_TYPE;

    last_error = RegSetValueEx(key, "EventMessageFile", 0, REG_SZ, exe_path,
                               sizeof(exe_path));

    if (ERROR_SUCCESS == last_error) {
      last_error =
          RegSetValueEx(key, "TypesSupported", 0, REG_DWORD,
                        (LPBYTE)&types_supported, sizeof(types_supported));
    }

    if (ERROR_SUCCESS != last_error) {
      std::cerr << "Failed to install source values: " << last_error << "\n";
    }

    RegCloseKey(key);
  } else {
    std::cerr << "Failed to install source: " << last_error << "\n";
  }
}

void log_event_log_message(const std::string& a_msg, const WORD a_type,
                           const std::string& a_name) {
  DWORD event_id;

  switch (a_type) {
    case EVENTLOG_ERROR_TYPE:
      event_id = MSG_ERROR_1;
      break;
    case EVENTLOG_WARNING_TYPE:
      event_id = MSG_WARNING_1;
      break;
    case EVENTLOG_INFORMATION_TYPE:
      event_id = MSG_INFO_1;
      break;
    default:
      std::cerr << "Unrecognised type: " << a_type << "\n";
      event_id = MSG_INFO_1;
      break;
  }

  HANDLE h_event_log = RegisterEventSource(0, a_name.c_str());

  if (0 == h_event_log) {
    std::cerr << "Failed open source '" << a_name << "': " << GetLastError()
              << "\n";
  } else {
    LPCTSTR message = a_msg.c_str();

    if (FALSE ==
        ReportEvent(h_event_log, a_type, 0, event_id, 0, 1, 0, &message, 0)) {
      std::cerr << "Failed to write message: " << GetLastError() << "\n";
    }

    DeregisterEventSource(h_event_log);
  }
}

void uninstall_event_log_source(const std::string& a_name) {
  const std::string key_path(
      "SYSTEM\\CurrentControlSet\\Services\\"
      "EventLog\\Application\\" +
      a_name);

  DWORD last_error = RegDeleteKey(HKEY_LOCAL_MACHINE, key_path.c_str());

  if (ERROR_SUCCESS != last_error) {
    std::cerr << "Failed to uninstall source: " << last_error << "\n";
  }
}

int main(int argc, char* argv[]) {
  PMIB_IFTABLE m_pTable = NULL;
  const std::string event_log_source_name("netmonitor");
  install_event_log_source(event_log_source_name);
  log_event_log_message("Netmonitor start", EVENTLOG_WARNING_TYPE,
                        event_log_source_name);

  if (GetIfTable(&m_pTable) == false) {
    return 1;
  }

  // Обход списка сетевых интерфейсов
  std::string buff;
  for (UINT i = 0; i < m_pTable->dwNumEntries; i++) {
    MIB_IFROW Row = m_pTable->table[i];
    char szDescr[MAXLEN_IFDESCR];
    memcpy(szDescr, Row.bDescr, Row.dwDescrLen);
    szDescr[Row.dwDescrLen] = 0;

    // Вывод собранной информации

    buff.append(szDescr);
    buff.append(":\n");
    buff.append("\tReceived: ");
    buff.append(Row.dwInOctets);
    buff.append(", Sent: ");
    buff.append(Row.dwOutOctets);
    buff.append("\n\n");
    log_event_log_message(buff.c_str(), EVENTLOG_INFORMATION_TYPE,
                          event_log_source_name);
    buff.clear();
  }

  // Завершение работы
  log_event_log_message("Netmonitor end", EVENTLOG_WARNING_TYPE,
                        event_log_source_name);
  delete (m_pTable);
  char a = getchar();
  return 0;
}

bool GetIfTable(PMIB_IFTABLE* m_pTable) {
  // Тип указателя на функцию GetIfTable
  typedef DWORD(_stdcall * TGetIfTable)(
      MIB_IFTABLE * pIfTable,  // Буфер таблицы интерфейсов
      ULONG * pdwSize,         // Размер буфера
      BOOL bOrder);            // Сортировать таблицу?

  // Пытаемся подгрузить iphlpapi.dll
  HINSTANCE iphlpapi;
  iphlpapi = LoadLibrary(L"iphlpapi.dll");
  if (!iphlpapi) {
    log_event_log_message("iphlpapi.dll not supported", EVENTLOG_ERROR_TYPE,
                          event_log_source_name);
    return false;
  }

  // Получаем адрес функции
  TGetIfTable pGetIfTable;
  pGetIfTable = (TGetIfTable)GetProcAddress(iphlpapi, "GetIfTable");

  // Получили требуемый размер буфера
  DWORD m_dwAdapters = 0;
  pGetIfTable(*m_pTable, &m_dwAdapters, TRUE);

  *m_pTable = new MIB_IFTABLE[m_dwAdapters];
  if (pGetIfTable(*m_pTable, &m_dwAdapters, TRUE) != ERROR_SUCCESS) {
    log_event_log_message("Error while GetIfTable", EVENTLOG_ERROR_TYPE,
                          event_log_source_name);
    delete *m_pTable;
    return false;
  }

  return true;
}
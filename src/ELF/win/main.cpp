#include <iostream>
#include <windows.h>
#include <iphlpapi.h>

bool GetIfTable(PMIB_IFTABLE *m_pTable);

int main(int argc, char *argv[]) {
  PMIB_IFTABLE m_pTable = NULL;

  if (GetIfTable(&m_pTable) == false) {
    return 1;
  }

  // Обход списка сетевых интерфейсов
  for (UINT i = 0; i < m_pTable->dwNumEntries; i++) {
    MIB_IFROW Row = m_pTable->table[i];
    char szDescr[MAXLEN_IFDESCR];
    memcpy(szDescr, Row.bDescr, Row.dwDescrLen);
    szDescr[Row.dwDescrLen] = 0;

    // Вывод собранной информации
    std::cout << szDescr << ":" << std::endl;
    std::cout << "\tReceived: " << Row.dwInOctets
              << ", Sent: " << Row.dwOutOctets << std::endl;
    std::cout << std::endl;
  }

  // Завершение работы
  delete (m_pTable);
  char a = getchar();
  return 0;
}

bool GetIfTable(PMIB_IFTABLE *m_pTable) {
  // Тип указателя на функцию GetIfTable
  typedef DWORD(_stdcall * TGetIfTable)(
      MIB_IFTABLE * pIfTable,  // Буфер таблицы интерфейсов
      ULONG * pdwSize,         // Размер буфера
      BOOL bOrder);            // Сортировать таблицу?

  // Пытаемся подгрузить iphlpapi.dll
  HINSTANCE iphlpapi;
  iphlpapi = LoadLibrary(L"iphlpapi.dll");
  if (!iphlpapi) {
    std::cerr << "iphlpapi.dll not supported" << std::endl;
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
    std::cerr << "Error while GetIfTable" << std::endl;
    delete *m_pTable;
    return false;
  }

  return true;
}
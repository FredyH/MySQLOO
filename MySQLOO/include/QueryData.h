#ifndef QUERY_DATA_
#define QUERY_DATA_
class QueryData
{
	int lastInsert;
	std::deque<my_ulonglong> m_affectedRows;
	std::deque<my_ulonglong> m_insertIds;
	std::deque<ResultData> results;
	std::condition_variable m_waitWakeupVariable;
}
#endif
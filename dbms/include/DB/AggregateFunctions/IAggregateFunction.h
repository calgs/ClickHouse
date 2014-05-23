#pragma once

#include <memory>

#include <Poco/SharedPtr.h>

#include <DB/Core/Row.h>
#include <DB/DataTypes/IDataType.h>


namespace DB
{


typedef char * AggregateDataPtr;
typedef const char * ConstAggregateDataPtr;


/** Интерфейс для агрегатных функций.
  * Экземпляры классов с этим интерфейсом не содержат самих данных для агрегации,
  *  а содержат лишь метаданные (описание) агрегатной функции,
  *  а также методы для создания, удаления и работы с данными.
  * Данные, получающиеся во время агрегации (промежуточные состояния вычислений), хранятся в других объектах
  *  (которые могут быть созданы в каком-нибудь пуле),
  *  а IAggregateFunction - внешний интерфейс для манипулирования ими.
  */
class IAggregateFunction
{
public:
	/// Получить основное имя функции.
	virtual String getName() const = 0;

	/** Указать типы аргументов. Если функция неприменима для данных аргументов - кинуть исключение.
	  * Необходимо вызывать перед остальными вызовами.
	  */
	virtual void setArguments(const DataTypes & arguments) = 0;

	/** Указать параметры - для параметрических агрегатных функций.
	  * Если параметры не предусмотрены или переданные параметры недопустимы - кинуть исключение.
	  * Если параметры есть - необходимо вызывать перед остальными вызовами, иначе - не вызывать.
	  */
	virtual void setParameters(const Array & params)
	{
		throw Exception("Aggregate function " + getName() + " doesn't allow parameters.",
			ErrorCodes::AGGREGATE_FUNCTION_DOESNT_ALLOW_PARAMETERS);
	}

	/// Получить тип результата.
	virtual DataTypePtr getReturnType() const = 0;

	virtual ~IAggregateFunction() {};


	/** Функции по работе с данными. */

	/** Создать пустые данные для агрегации с помощью placement new в заданном месте.
	  * Вы должны будете уничтожить их с помощью метода destroy.
	  */
	virtual void create(AggregateDataPtr place) const = 0;

	/// Уничтожить данные для агрегации.
	virtual void destroy(AggregateDataPtr place) const noexcept = 0;

	/// Уничтожать данные не обязательно.
	virtual bool hasTrivialDestructor() const = 0;

	/// Получить sizeof структуры с данными.
	virtual size_t sizeOfData() const = 0;

	/// Как должна быть выровнена структура с данными. NOTE: Сейчас не используется (структуры с состоянием агрегации кладутся без выравнивания).
	virtual size_t alignOfData() const = 0;

	/// Добавить значение. columns - столбцы, содержащие аргументы, row_num - номер строки в столбцах.
	virtual void add(AggregateDataPtr place, const IColumn ** columns, size_t row_num) const = 0;

	/// Объединить состояние с другим состоянием.
	virtual void merge(AggregateDataPtr place, ConstAggregateDataPtr rhs) const = 0;

	/// Сериализовать состояние (например, для передачи по сети). Нельзя сериализовывать "пустое" состояние.
	virtual void serialize(ConstAggregateDataPtr place, WriteBuffer & buf) const = 0;

	/// Десериализовать состояние и объединить своё состояние с ним.
	virtual void deserializeMerge(AggregateDataPtr place, ReadBuffer & buf) const = 0;

	/// Сериализовать состояние в текстовом виде (а не в бинарном, как в функции serialize). Нельзя сериализовывать "пустое" состояние.
	virtual void serializeText(ConstAggregateDataPtr place, WriteBuffer & buf) const
	{
		throw Exception("Method serializeText is not supported for " + getName() + ".", ErrorCodes::NOT_IMPLEMENTED);
	}

	/// Десериализовать текстовое состояние и объединить своё состояние с ним.
	virtual void deserializeMergeText(AggregateDataPtr place, ReadBuffer & buf) const
	{
		throw Exception("Method deserializeMergeText is not supported for " + getName() + ".", ErrorCodes::NOT_IMPLEMENTED);
	}

	/// Вставить результат в столбец.
	virtual void insertResultInto(ConstAggregateDataPtr place, IColumn & to) const = 0;

	/// Можно ли вызывать метод insertResultInto, или всегда нужно запоминать состояние.
	virtual bool canBeFinal() const { return true; }
};


/// Реализует несколько методов. T - тип структуры с данными для агрегации.
template <typename T>
class IAggregateFunctionHelper : public IAggregateFunction
{
protected:
	typedef T Data;
	
	static Data & data(AggregateDataPtr place) 				{ return *reinterpret_cast<Data*>(place); }
	static const Data & data(ConstAggregateDataPtr place) 	{ return *reinterpret_cast<const Data*>(place); }
	
public:
	void create(AggregateDataPtr place) const
	{
		new (place) Data;
	}

	void destroy(AggregateDataPtr place) const noexcept
	{
		data(place).~Data();
	}

	bool hasTrivialDestructor() const
	{
		return __has_trivial_destructor(Data);
	}

	size_t sizeOfData() const
	{
		return sizeof(Data);
	}

	/// NOTE: Сейчас не используется (структуры с состоянием агрегации кладутся без выравнивания).
	size_t alignOfData() const
	{
		return __alignof__(Data);
	}
};


using Poco::SharedPtr;

typedef SharedPtr<IAggregateFunction> AggregateFunctionPtr;

}

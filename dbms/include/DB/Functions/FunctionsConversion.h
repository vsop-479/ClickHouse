#pragma once

#include <DB/IO/WriteBufferFromVector.h>
#include <DB/IO/ReadBufferFromString.h>
#include <DB/DataTypes/DataTypesNumberFixed.h>
#include <DB/DataTypes/DataTypeString.h>
#include <DB/DataTypes/DataTypeFixedString.h>
#include <DB/DataTypes/DataTypeDate.h>
#include <DB/DataTypes/DataTypeDateTime.h>
#include <DB/Columns/ColumnString.h>
#include <DB/Columns/ColumnFixedString.h>
#include <DB/Columns/ColumnConst.h>
#include <DB/Functions/IFunction.h>


namespace DB
{

/** Функции преобразования типов.
  *
  * Бывают двух видов:
  * - toType - преобразование "естественным образом"; TODO: преобразования из/в FixedString.
  * - TODO: reinterpretAsType - преобразования чисел и дат в строки, содержащие тот же набор байт в машинном представлении, и наоборот.
  */


/** Преобразование чисел друг в друга, дат/дат-с-временем в числа и наоборот: делается обычным присваиванием.
  *  (дата внутри хранится как количество дней с какого-то, дата-с-временем - как unix timestamp)
  */
template <typename FromDataType, typename ToDataType, typename Name>
struct ConvertImpl
{
	typedef typename FromDataType::FieldType FromFieldType;
	typedef typename ToDataType::FieldType ToFieldType;
	
	static void execute(Block & block, const ColumnNumbers & arguments, size_t result)
	{
		if (const ColumnVector<FromFieldType> * col_from = dynamic_cast<const ColumnVector<FromFieldType> *>(&*block.getByPosition(arguments[0]).column))
		{
			ColumnVector<ToFieldType> * col_to = new ColumnVector<ToFieldType>;
			block.getByPosition(result).column = col_to;

			const typename ColumnVector<FromFieldType>::Container_t & vec_from = col_from->getData();
			typename ColumnVector<ToFieldType>::Container_t & vec_to = col_to->getData();
			size_t size = vec_from.size();
			vec_to.resize(size);

			for (size_t i = 0; i < size; ++i)
				vec_to[i] = vec_from[i];
		}
		else if (const ColumnConst<FromFieldType> * col_from = dynamic_cast<const ColumnConst<FromFieldType> *>(&*block.getByPosition(arguments[0]).column))
		{
			block.getByPosition(result).column = new ColumnConst<ToFieldType>(col_from->size(), col_from->getData());
		}
		else
			throw Exception("Illegal column " + block.getByPosition(arguments[0]).column->getName()
					+ " of first argument of function " + Name::get(),
				ErrorCodes::ILLEGAL_COLUMN);
	}
};


/** Преобразование даты в дату-с-временем: добавление нулевого времени.
  */
template <typename Name>
struct ConvertImpl<DataTypeDate, DataTypeDateTime, Name>
{
	typedef DataTypeDate::FieldType FromFieldType;
	typedef DataTypeDateTime::FieldType ToFieldType;

	static void execute(Block & block, const ColumnNumbers & arguments, size_t result)
	{
		typedef DataTypeDate::FieldType FromFieldType;
		Yandex::DateLUTSingleton & date_lut = Yandex::DateLUTSingleton::instance();

		if (const ColumnVector<FromFieldType> * col_from = dynamic_cast<const ColumnVector<FromFieldType> *>(&*block.getByPosition(arguments[0]).column))
		{
			ColumnVector<ToFieldType> * col_to = new ColumnVector<ToFieldType>;
			block.getByPosition(result).column = col_to;

			const typename ColumnVector<FromFieldType>::Container_t & vec_from = col_from->getData();
			typename ColumnVector<ToFieldType>::Container_t & vec_to = col_to->getData();
			size_t size = vec_from.size();
			vec_to.resize(size);

			for (size_t i = 0; i < size; ++i)
			{
				vec_to[i] = date_lut.safeFromDayNum(Yandex::DayNum_t(vec_from[i]));
			}
		}
		else if (const ColumnConst<FromFieldType> * col_from = dynamic_cast<const ColumnConst<FromFieldType> *>(&*block.getByPosition(arguments[0]).column))
		{
			block.getByPosition(result).column = new ColumnConst<ToFieldType>(col_from->size(), date_lut.safeFromDayNum(Yandex::DayNum_t(col_from->getData())));
		}
		else
			throw Exception("Illegal column " + block.getByPosition(arguments[0]).column->getName()
					+ " of first argument of function " + Name::get(),
				ErrorCodes::ILLEGAL_COLUMN);
	}
};


/** Преобразование даты-с-временем в дату: отбрасывание времени.
  */
template <typename Name>
struct ConvertImpl<DataTypeDateTime, DataTypeDate, Name>
{
	typedef DataTypeDateTime::FieldType FromFieldType;
	typedef DataTypeDate::FieldType ToFieldType;

	static void execute(Block & block, const ColumnNumbers & arguments, size_t result)
	{
		Yandex::DateLUTSingleton & date_lut = Yandex::DateLUTSingleton::instance();

		if (const ColumnVector<FromFieldType> * col_from = dynamic_cast<const ColumnVector<FromFieldType> *>(&*block.getByPosition(arguments[0]).column))
		{
			ColumnVector<ToFieldType> * col_to = new ColumnVector<ToFieldType>;
			block.getByPosition(result).column = col_to;

			const typename ColumnVector<FromFieldType>::Container_t & vec_from = col_from->getData();
			typename ColumnVector<ToFieldType>::Container_t & vec_to = col_to->getData();
			size_t size = vec_from.size();
			vec_to.resize(size);

			for (size_t i = 0; i < size; ++i)
				vec_to[i] = date_lut.safeToDayNum(vec_from[i]);
		}
		else if (const ColumnConst<FromFieldType> * col_from = dynamic_cast<const ColumnConst<FromFieldType> *>(&*block.getByPosition(arguments[0]).column))
		{
			block.getByPosition(result).column = new ColumnConst<ToFieldType>(col_from->size(), date_lut.safeToDayNum(col_from->getData()));
		}
		else
			throw Exception("Illegal column " + block.getByPosition(arguments[0]).column->getName()
					+ " of first argument of function " + Name::get(),
				ErrorCodes::ILLEGAL_COLUMN);
	}
};


/** Преобразование чисел, дат, дат-с-временем в строки: через форматирование.
  */
template <typename DataType> void formatImpl(typename DataType::FieldType x, WriteBuffer & wb) { writeText(x, wb); }
template <> inline void formatImpl<DataTypeDate>(DataTypeDate::FieldType x, WriteBuffer & wb) { writeDateText(Yandex::DayNum_t(x), wb); }
template <> inline void formatImpl<DataTypeDateTime>(DataTypeDateTime::FieldType x, WriteBuffer & wb) { writeDateTimeText(x, wb); }

template <typename FromDataType, typename Name>
struct ConvertImpl<FromDataType, DataTypeString, Name>
{
	typedef typename FromDataType::FieldType FromFieldType;

	static void execute(Block & block, const ColumnNumbers & arguments, size_t result)
	{
		if (const ColumnVector<FromFieldType> * col_from = dynamic_cast<const ColumnVector<FromFieldType> *>(&*block.getByPosition(arguments[0]).column))
		{
			ColumnString * col_to = new ColumnString;
			block.getByPosition(result).column = col_to;

			const typename ColumnVector<FromFieldType>::Container_t & vec_from = col_from->getData();
			ColumnUInt8::Container_t & data_to = dynamic_cast<ColumnUInt8 &>(col_to->getData()).getData();
			ColumnString::Offsets_t & offsets_to = col_to->getOffsets();
			size_t size = vec_from.size();
			data_to.resize(size * 2);
			offsets_to.resize(size);

			WriteBufferFromVector<UInt8, PODArray<UInt8> > write_buffer(data_to);

			for (size_t i = 0; i < size; ++i)
			{
				formatImpl<FromDataType>(vec_from[i], write_buffer);
				writeChar(0, write_buffer);
				offsets_to[i] = write_buffer.count();
			}
			data_to.resize(write_buffer.count());
		}
		else if (const ColumnConst<FromFieldType> * col_from = dynamic_cast<const ColumnConst<FromFieldType> *>(&*block.getByPosition(arguments[0]).column))
		{
			std::vector<char> buf;
			WriteBufferFromVector<char> write_buffer(buf);
			formatImpl<FromDataType>(col_from->getData(), write_buffer);
			block.getByPosition(result).column = new ColumnConstString(col_from->size(), std::string(&buf[0], write_buffer.count()));
		}
		else
			throw Exception("Illegal column " + block.getByPosition(arguments[0]).column->getName()
					+ " of first argument of function " + Name::get(),
				ErrorCodes::ILLEGAL_COLUMN);
	}
};


/** Преобразование строк в числа, даты, даты-с-временем: через парсинг.
  */
template <typename DataType> void parseImpl(typename DataType::FieldType & x, ReadBuffer & rb) { readText(x,rb); }

template <> inline void parseImpl<DataTypeDate>(DataTypeDate::FieldType & x, ReadBuffer & rb)
{
	Yandex::DayNum_t tmp(0);
	readDateText(tmp, rb);
	x = tmp;
}

template <> inline void parseImpl<DataTypeDateTime>(DataTypeDateTime::FieldType & x, ReadBuffer & rb)
{
	time_t tmp = 0;
	readDateTimeText(tmp, rb);
	x = tmp;
}

template <typename ToDataType, typename Name>
struct ConvertImpl<DataTypeString, ToDataType, Name>
{
	typedef typename ToDataType::FieldType ToFieldType;

	static void execute(Block & block, const ColumnNumbers & arguments, size_t result)
	{
		if (const ColumnString * col_from = dynamic_cast<const ColumnString *>(&*block.getByPosition(arguments[0]).column))
		{
			ColumnVector<ToFieldType> * col_to = new ColumnVector<ToFieldType>;
			block.getByPosition(result).column = col_to;

			const ColumnUInt8::Container_t & data_from = dynamic_cast<const ColumnUInt8 &>(col_from->getData()).getData();
			typename ColumnVector<ToFieldType>::Container_t & vec_to = col_to->getData();
			size_t size = col_from->size();
			vec_to.resize(size);

			ReadBuffer read_buffer(const_cast<char *>(reinterpret_cast<const char *>(&data_from[0])), data_from.size(), 0);

			char zero = 0;
			for (size_t i = 0; i < size; ++i)
			{
				parseImpl<ToDataType>(vec_to[i], read_buffer);
				readChar(zero, read_buffer);
				if (zero != 0)
					throw Exception("Cannot parse number from string.", ErrorCodes::CANNOT_PARSE_NUMBER);
			}
		}
		else if (const ColumnConstString * col_from = dynamic_cast<const ColumnConstString *>(&*block.getByPosition(arguments[0]).column))
		{
			const String & s = col_from->getData();
			ReadBufferFromString read_buffer(s);
			ToFieldType x = 0;
			parseImpl<ToDataType>(x, read_buffer);
			block.getByPosition(result).column = new ColumnConst<ToFieldType>(col_from->size(), x);
		}
		else
			throw Exception("Illegal column " + block.getByPosition(arguments[0]).column->getName()
					+ " of first argument of function " + Name::get(),
				ErrorCodes::ILLEGAL_COLUMN);
	}
};


/** Если типы совпадают - просто скопируем ссылку на столбец.
  */
template <typename Name>
struct ConvertImpl<DataTypeString, DataTypeString, Name>
{
	static void execute(Block & block, const ColumnNumbers & arguments, size_t result)
	{
		block.getByPosition(result).column = block.getByPosition(arguments[0]).column;
	}
};



template <typename ToDataType, typename Name>
class FunctionConvert : public IFunction
{
public:
	/// Получить имя функции.
	String getName() const
	{
		return Name::get();
	}

	/// Получить тип результата по типам аргументов. Если функция неприменима для данных аргументов - кинуть исключение.
	DataTypePtr getReturnType(const DataTypes & arguments) const
	{
		if (arguments.size() != 1)
			throw Exception("Number of arguments for function " + getName() + " doesn't match: passed "
				+ Poco::NumberFormatter::format(arguments.size()) + ", should be 1.",
				ErrorCodes::NUMBER_OF_ARGUMENTS_DOESNT_MATCH);

		return new ToDataType;
	}

	/// Выполнить функцию над блоком.
	void execute(Block & block, const ColumnNumbers & arguments, size_t result)
	{
		IDataType * from_type = &*block.getByPosition(arguments[0]).type;
		
		if      (dynamic_cast<const DataTypeUInt8 *		>(from_type)) ConvertImpl<DataTypeUInt8, 	ToDataType, Name>::execute(block, arguments, result);
		else if (dynamic_cast<const DataTypeUInt16 *	>(from_type)) ConvertImpl<DataTypeUInt16, 	ToDataType, Name>::execute(block, arguments, result);
		else if (dynamic_cast<const DataTypeUInt32 *	>(from_type)) ConvertImpl<DataTypeUInt32, 	ToDataType, Name>::execute(block, arguments, result);
		else if (dynamic_cast<const DataTypeUInt64 *	>(from_type)) ConvertImpl<DataTypeUInt64, 	ToDataType, Name>::execute(block, arguments, result);
		else if (dynamic_cast<const DataTypeInt8 *		>(from_type)) ConvertImpl<DataTypeInt8, 	ToDataType, Name>::execute(block, arguments, result);
		else if (dynamic_cast<const DataTypeInt16 *		>(from_type)) ConvertImpl<DataTypeInt16, 	ToDataType, Name>::execute(block, arguments, result);
		else if (dynamic_cast<const DataTypeInt32 *		>(from_type)) ConvertImpl<DataTypeInt32, 	ToDataType, Name>::execute(block, arguments, result);
		else if (dynamic_cast<const DataTypeInt64 *		>(from_type)) ConvertImpl<DataTypeInt64, 	ToDataType, Name>::execute(block, arguments, result);
		else if (dynamic_cast<const DataTypeFloat32 *	>(from_type)) ConvertImpl<DataTypeFloat32, 	ToDataType, Name>::execute(block, arguments, result);
		else if (dynamic_cast<const DataTypeFloat64 *	>(from_type)) ConvertImpl<DataTypeFloat64, 	ToDataType, Name>::execute(block, arguments, result);
		else if (dynamic_cast<const DataTypeDate *		>(from_type)) ConvertImpl<DataTypeDate, 	ToDataType, Name>::execute(block, arguments, result);
		else if (dynamic_cast<const DataTypeDateTime *	>(from_type)) ConvertImpl<DataTypeDateTime,	ToDataType, Name>::execute(block, arguments, result);
		else if (dynamic_cast<const DataTypeString *	>(from_type)) ConvertImpl<DataTypeString, 	ToDataType, Name>::execute(block, arguments, result);
		else
			throw Exception("Illegal type " + block.getByPosition(arguments[0]).type->getName() + " of argument of function " + getName(),
				ErrorCodes::ILLEGAL_TYPE_OF_ARGUMENT);
	}
};


struct NameToUInt8 			{ static const char * get() { return "toUInt8"; } };
struct NameToUInt16 		{ static const char * get() { return "toUInt16"; } };
struct NameToUInt32 		{ static const char * get() { return "toUInt32"; } };
struct NameToUInt64 		{ static const char * get() { return "toUInt64"; } };
struct NameToInt8 			{ static const char * get() { return "toInt8"; } };
struct NameToInt16	 		{ static const char * get() { return "toInt16"; } };
struct NameToInt32			{ static const char * get() { return "toInt32"; } };
struct NameToInt64			{ static const char * get() { return "toInt64"; } };
struct NameToFloat32		{ static const char * get() { return "toFloat32"; } };
struct NameToFloat64		{ static const char * get() { return "toFloat64"; } };
struct NameToDate			{ static const char * get() { return "toDate"; } };
struct NameToDateTime		{ static const char * get() { return "toDateTime"; } };
struct NameToString			{ static const char * get() { return "toString"; } };

typedef FunctionConvert<DataTypeUInt8,		NameToUInt8> 		FunctionToUInt8;
typedef FunctionConvert<DataTypeUInt16,		NameToUInt16> 		FunctionToUInt16;
typedef FunctionConvert<DataTypeUInt32,		NameToUInt32> 		FunctionToUInt32;
typedef FunctionConvert<DataTypeUInt64,		NameToUInt64> 		FunctionToUInt64;
typedef FunctionConvert<DataTypeInt8,		NameToInt8> 		FunctionToInt8;
typedef FunctionConvert<DataTypeInt16,		NameToInt16> 		FunctionToInt16;
typedef FunctionConvert<DataTypeInt32,		NameToInt32> 		FunctionToInt32;
typedef FunctionConvert<DataTypeInt64,		NameToInt64> 		FunctionToInt64;
typedef FunctionConvert<DataTypeFloat32,	NameToFloat32> 		FunctionToFloat32;
typedef FunctionConvert<DataTypeFloat64,	NameToFloat64> 		FunctionToFloat64;
typedef FunctionConvert<DataTypeDate,		NameToDate> 		FunctionToDate;
typedef FunctionConvert<DataTypeDateTime,	NameToDateTime> 	FunctionToDateTime;
typedef FunctionConvert<DataTypeString,		NameToString> 		FunctionToString;


}

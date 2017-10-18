# @title Number Type Mapping between Oracle and Ruby

Number Type Mapping between Oracle and Ruby
===========================================

Default mapping
---------------

Oracle numbers in select statements are fetched as followings by default:

| Oracle Data Type | Ruby Class |
|---|---|
| NUMBER(prec) or NUMBER(prec, 0) | Integer |
| NUMBER(prec, scale) where prec < 15 and scale != 0 | Float |
| NUMBER(prec, scale) where prec >= 15 and scale != 0 | BigDecimal |
| FLOAT or FLOAT(prec) | Float |
| NUMBER without precision and scale | BigDecimal |
| number type returned by functions or calculated number | BigDecimal |
| BINARY_FLOAT | Float |
| BINARY_DOUBLE | Float |

When the data type is within Integer or Float class, it is fetched
as Integer or Float. Otherwise, BigDecimal.

Note that the mapping is determined by the column definition in
select statements, not by the actual value fetched.
For example the column in `select count(*) from table_name` is
fetched as BigDecimal because it is returned from `count` function.

The mapping is customizable by `OCI8::BindType::Mapping`.
The default values of Oracle number data type mapping are:

    # NUMBER or FLOAT data type, used for the first six rows in the above table
    OCI8::BindType::Mapping[:number] = OCI8::BindType::Number
    # BINARY_FLOAT data type, used for the seventh row in the above table
    OCI8::BindType::Mapping[:binary_float] = OCI8::BindType::BinaryDouble
    # BINARY_DOUBLE data type, used for the eighth row in the above table
    OCI8::BindType::Mapping[:binary_double] = OCI8::BindType::BinaryDouble

`OCI8::BindType::Number` checks precision and scale to determine
ruby class. The first four rows in the above table are hard-coded.
The fifth and sixth rows are, however, customizable by
`OCI8::BindType::Mapping[:number_no_prec_setting]` and
`OCI8::BindType::Mapping[:number_unknown_prec]` respectively.

The default values are:

    OCI8::BindType::Mapping[:number_no_prec_setting] = OCI8::BindType::BigDecimal
    OCI8::BindType::Mapping[:number_unknown_prec] = OCI8::BindType::BigDecimal

The mapping may be changed as follows in future.

| Oracle Data Type | Ruby Class |
|---|---|
| NUMBER(prec) or NUMBER(prec, 0) | Integer |
| other NUMBER  | OraNumber |
| BINARY_FLOAT | Float |
| BINARY_DOUBLE | Float |

Customize mapping
-----------------

Add the following code to fetch all number or float columns as {OraNumber}.

    OCI8::BindType::Mapping[:number] = OCI8::BindType::OraNumber

Otherwise, add the following code to customize the fifth and sixth rows only
in the above table.

    OCI8::BindType::Mapping[:number_no_prec_setting] = OCI8::BindType::OraNumber
    OCI8::BindType::Mapping[:number_unknown_prec] = OCI8::BindType::OraNumber

If you want to fetch numbers as Integer or Float by its actual value, use
the following code:

    # Fetch numbers as Integer when fetched number doesn't has
    # fractional part. Otherwise, Float.
    # For example when a column contains 10.0 and 10.1, they are
    # fetched as Integer and Float respectively.
    OCI8::BindType::Mapping[:number] = OCI8::BindType::BasicNumberType

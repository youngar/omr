/*******************************************************************************
 *
 * (c) Copyright IBM Corp. 2015, 2016
 *
 *  This program and the accompanying materials are made available
 *  under the terms of the Eclipse Public License v1.0 and
 *  Apache License v2.0 which accompanies this distribution.
 *
 *      The Eclipse Public License is available at
 *      http://www.eclipse.org/legal/epl-v10.html
 *
 *      The Apache License v2.0 is available at
 *      http://www.opensource.org/licenses/apache2.0.php
 *
 * Contributors:
 *    Multiple authors (IBM Corp.) - initial implementation and documentation
 *******************************************************************************/

#include <stdint.h>
#include <iostream>

struct S3 {
	//volatile field
	volatile uint8_t q;
	//const field
	const uint8_t r;
};
struct S3 instanceOfS3 = {1, 4};

//typedef of pointer type
typedef S3 *pS3;

//multiple inheritance
class Vehicle
{
public:
	double speed;
	Vehicle(double speedIn) : speed(speedIn) {}
};

class Toy
{
public:
	int durability;
	Toy(int durabilityIn) : durability(durabilityIn) {}
};

class MatchboxCar : public Vehicle, public Toy
{
public:
	int hasWheels;
	MatchboxCar(int hasWheelsIn, double speedIn, int durabilityIn) : Vehicle(speedIn), Toy(durabilityIn), hasWheels(hasWheelsIn) {}
};

/* Function pointer */
typedef void VOID;
typedef VOID* (*FunctionPointer)();

/* Array and pointer typedef test */
typedef int * INT[2];
typedef INT * INT_ARRAY[10];

struct StructWithFunctionPointer {
	FunctionPointer functionPointer;
	INT_ARRAY i;
};
struct StructWithFunctionPointer structFunctionPointer;

//struct with base types
struct S4 {
	int i;
	long l;
	double d;
	char c;
	bool b;
};
struct S4 instanceOfS4 = {0, 1L, 3.8, 'c', true};

//Multiple untagged struct definitions inside a scope.
struct Outer {
	struct {
		uint8_t ui8;
	} innerField1;
	struct {
		int16_t i16;
	} innerField2;
	struct {
		struct InnerStruct {
			int32_t i32;
		};
		InnerStruct innerStruct;
	} innerField3;
};
struct Outer instanceOfOuter = {{2}, {4}, {{6}}};

//Two different class's with inner class's with the same name.
class OuterClassWithDifferentName1
{
private:
	class InnerClassWithSameName
	{
	public:
		double g;
	};
	InnerClassWithSameName instanceOfInnerClassWithSameName;
public:
};
OuterClassWithDifferentName1 instanceOfOuterClassWithDifferentName1;

class OuterClassWithDifferentName2
{
private:
	class InnerClassWithSameName
	{
	public:
		double h;
	};
	InnerClassWithSameName instanceOfInnerClassWithSameName;
public:
};
OuterClassWithDifferentName2 instanceOfOuterClassWithDifferentName2;

//Template class
template <class T>
class Complex
{
	T real, imaginary;
public:
	Complex(T realIn, T imaginaryIn)
	{
		real = realIn;
		imaginary = imaginaryIn;
	}
};
Complex <int> instanceOfComplex1(100, 75);
Complex <double> instanceOfComplex2(64.829, 23.42);

//Array field with base type defined in a typedef
typedef unsigned int UDATA2;
UDATA2 instanceOfUDATA2;

struct SArrayWithTypedefType {
	UDATA2 arrayWithTypedefType[12];
};
struct SArrayWithTypedefType instanceOfSArrayWithTypedefType = {{1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12}};

void
sample3()
{
	std::cout << "\ninstanceOfS3.q: " << (unsigned int)instanceOfS3.q << std::endl;
	std::cout << "instanceOfS3.r: " << (unsigned int)instanceOfS3.r << std::endl;

	instanceOfS3.q = 2;

	pS3 ps3 = &instanceOfS3;

	std::cout << "ps3->q: " << (unsigned int)ps3->q << std::endl;
	std::cout << "ps3->r: " << (unsigned int)ps3->r << std::endl;

	MatchboxCar matchboxCar(1, 10.7, 10);

	std::cout << "matchboxCar.speed: " << (double)matchboxCar.speed << std::endl;
	std::cout << "matchboxCar.durability: " << matchboxCar.durability << std::endl;
	std::cout << "matchboxCar.hasWheels: " << matchboxCar.hasWheels << std::endl;

	std::cout << "functionPointer: " << structFunctionPointer.functionPointer << std::endl;
}

// MCRE.cpp : Defines the entry point for the application.
//

#include "mcre/core/errors.hpp"
#include<iostream>

using namespace std;

int main()
{
	try {
		MCRE_TODO("Hello");

	}
	catch (const mcre::NotImplementedError& e) {
		cout << e.what() << endl;
	}
	cin.get();
	return 0;
}

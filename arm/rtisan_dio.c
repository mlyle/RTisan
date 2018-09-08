#include <rtisan_dio.h>

void DIOInit(DIOInitTag_t t)
{
	/* Empty value meaning "no pin" is OK */
	if (t == DIO_NULL) {
		return;
	}

	enum DIOPinFunc func = GET_DIO_FUNC(t);

	switch (func) {
		case DIO_PIN_INPUT:
			DIOSetInput(t, GET_DIO_PULL(t));
			break;
		case DIO_PIN_OUTPUT:
			DIOSetOutput(t, GET_DIO_OD(t),
					GET_DIO_DRIVE(t),
					GET_DIO_INITIAL(t));
			break;
		case DIO_PIN_ALTFUNC_IN:
			DIOSetAltfuncInput(t, GET_DIO_ALTFUNC(t),
					GET_DIO_PULL(t));
			break;
		case DIO_PIN_ALTFUNC_OUT:
			DIOSetAltfuncOutput(t, GET_DIO_ALTFUNC(t),
					GET_DIO_OD(t),
					GET_DIO_DRIVE(t));
			break;
		case DIO_PIN_ANALOG:
			DIOSetAnalog(t);
			break;
	}
}


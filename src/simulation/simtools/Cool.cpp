#include "ToolClasses.h"
//#TPT-Directive ToolClass Tool_Cool TOOL_COOL 1
Tool_Cool::Tool_Cool()
{
	Identifier = "DEFAULT_TOOL_COOL";
	Name = "COOL";
	Colour = PIXPACK(0x00DDFF);
	Description = "Cools the targeted element.";
}

int Tool_Cool::Perform(Simulation * sim, Particle * cpart, int x, int y, int brushX, int brushY, float strength)
{
	if(!cpart)
		return 0;
	if (cpart->type == PT_PUMP || cpart->type == PT_GPMP)
		cpart->temp -= (UFixed)(strength*.1f);
	else
		cpart->temp -= (UFixed)(strength*2.0f);

	cpart->temp = restrict_flt(cpart->temp, (UFixed)MIN_TEMP, (UFixed)MAX_TEMP);
	return 1;
}

Tool_Cool::~Tool_Cool() {}

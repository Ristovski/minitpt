#include "ToolClasses.h"
//#TPT-Directive ToolClass Tool_Heat TOOL_HEAT 0
Tool_Heat::Tool_Heat()
{
	Identifier = "DEFAULT_TOOL_HEAT";
	Name = "HEAT";
	Colour = PIXPACK(0xFFDD00);
	Description = "Heats the targeted element.";
}

int Tool_Heat::Perform(Simulation * sim, Particle * cpart, int x, int y, int brushX, int brushY, float strength)
{
	if(!cpart)
		return 0;
	if (cpart->type == PT_PUMP || cpart->type == PT_GPMP)
		cpart->temp += (UFixed)(strength*.1f);
	else
		cpart->temp += (UFixed)(strength*2.0f);

	cpart->temp = restrict_flt(cpart->temp, (UFixed)MIN_TEMP, (UFixed)MAX_TEMP);
	return 1;
}

Tool_Heat::~Tool_Heat() {}

# Import existing libraries and all classes from the classes folder
from classes.pilot_assistant_system import PilotAssistantSystem


if __name__ == "__main__":
    system = PilotAssistantSystem()
    system.run()
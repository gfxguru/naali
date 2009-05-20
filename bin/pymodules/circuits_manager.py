"""drafting and designing a manager for python written modules.
the current plan is that there'll be a Manager that the PythonScriptModule
instanciates, and which then loads the python modules that are to be autoloaded,
and handles their event registrations, passing incoming events to them etc.
that will be prototyped here in pure py, and perhaps reused in the actual
impl if it seems that a py written manager makes sense within the c++ framework too.

currently here is only test code for modules and handelers that would
use the manager, nothing of the Manager itself yet.
"""

from circuits import handler, Event, Component, Manager

class Update(Event): pass     
class Chat(Event): pass    
class Input(Event): pass
    
class ComponentRunner(Component):
    instance = None

    def __init__(self):
        # instanciated from the c++ side, as modulemanager there
        assert self.instance is None
        ComponentRunner.instance = self #is used as a singleton now

        # Create a new circuits Manager
        self.m = Manager()

        Component.__init__(self)
        
        m = self.m
        
        #XXX make some 'autoload' system, perhaps just 'import autoload'?
        #or __all__ in pymodules __init__ ? (i.e. import pymodules would do that)
        #dunno yet, but something. next TODO basically (task xyz)
        import circuits_testmodule
        tm = circuits_testmodule.TestModule()
        m += tm # Equivalent to: tm.register(m)
        
        m.start()

    def run(self, deltatime=0.1):
        #print ".",
        self.m.push(Update(deltatime), "update")
        
    def RexNetMsgChatFromSimulator(self, frm, message):
        frm = "Bob"
        self.m.push(Chat(frm, message), "on_chat")
        
    def INPUT_EVENT(self, evid):
        """Note: the PygameDriver for circuits has a different design:
        there the event data is Key, and the channel either "keydown" or "keyup",
        and mouse and clicks are different events and channels.
        Here we have no way to differentiate presses/releases,
        'cause the c++ internals don't do that apart from the constant name.
        """
        self.m.push(Input(evid), "on_input")
        #print "circuits_manager ComponentRunner got input event:", evid        
        
    def exit(self):
        print "Circuits manager stopping."
        self.m.stop()
        
#TestModule moved to own file (err, module)

"""
why not allow plain functions as event handlers too,
the python module of it, i.e. the file, can be considered as the module,
so no classes are required.

circuits has it so that you are supposed to make components.
"""

#~ @e.communication.PRESENCE_STATUS_UPDATE
#~ def sparkle_presence_updates():
    #~ """a custom visual thing: some local sparkles upon IM presence updates.
    #~ what data do we have in these events? the im participant id?
    #~ is that associated to an avatar anywhere? so could e.g. get the location..
    #~ """
    #~ create_sparkle((0,2, 0,2), 0.5) #in upper right corner, for half a sec
    
if __name__ == '__main__':
    runner = ComponentRunner()

    import time
    while True:
        runner.run(0.1)
        runner.RexNetMsgChatFromSimulator("main..")
        time.sleep(0.1)



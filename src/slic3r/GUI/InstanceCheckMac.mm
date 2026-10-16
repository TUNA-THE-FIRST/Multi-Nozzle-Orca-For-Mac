#import "InstanceCheck.hpp"
#import "InstanceCheckMac.h"
#import "GUI_App.hpp"

@implementation OtherInstanceMessageHandlerMac

-(instancetype) init
{
	self = [super init];
	return self;
}
-(void)add_observer:(NSString *)version_hash
{
	//NSLog(@"adding observer");
	//NSString *nsver = @"OtherSnapmaker_OrcaInstanceMessage" + version_hash;
	NSString *nsver = [NSString stringWithFormat: @"%@%@", @"OtherSnapmaker_OrcaInstanceMessage", version_hash];
	[[NSDistributedNotificationCenter defaultCenter] addObserver:self selector:@selector(message_update:) name:nsver object:nil suspensionBehavior:NSNotificationSuspensionBehaviorDeliverImmediately];
	NSString *nsver2 = [NSString stringWithFormat: @"%@%@", @"OtherSnapmaker_OrcaInstanceClosing", version_hash];
	[[NSDistributedNotificationCenter defaultCenter] addObserver:self selector:@selector(closing_update:) name:nsver2 object:nil suspensionBehavior:NSNotificationSuspensionBehaviorDeliverImmediately];
}

-(void)message_update:(NSNotification *)msg
{
	[self bring_forward];
	//pass message  
	NSString *data = msg.userInfo[@"data"];
	if (![data isKindOfClass:[NSString class]])
		return;
	Slic3r::GUI::wxGetApp().other_instance_message_handler()->handle_message(std::string([data UTF8String]));
}

-(void)closing_update:(NSNotification *)msg
{
	//[self bring_forward];
	//pass message  
	Slic3r::GUI::wxGetApp().other_instance_message_handler()->handle_message_other_closed();
}

-(void) bring_forward
{
	//demiaturize all windows
	for(NSWindow* win in [NSApp windows])
	{
		if([win isMiniaturized])
		{
			[win deminiaturize:self];
		}
	}
	//bring window to front 
	[[NSApplication sharedApplication] activateIgnoringOtherApps : YES];
}

@end

namespace Slic3r {

// Messages carry file paths, so they travel as UTF-8: the legacy default C string encoding (Mac Roman)
// garbled names with accents ("peça.3mf"), and the running instance could not find the file.
void send_message_mac(const std::string &msg, const std::string &version)
{
	NSString *nsmsg = [NSString stringWithUTF8String:msg.c_str()];
	if (nsmsg == nil) // not valid UTF-8; a nil value would throw in dictionaryWithObject:
		return;
	//NSString *nsver = @"OtherSnapmaker_OrcaInstanceMessage" + [NSString stringWithCString:version.c_str() encoding:[NSString defaultCStringEncoding]];
	NSString *nsver = [NSString stringWithUTF8String:version.c_str()];
	NSString *notifname = [NSString stringWithFormat: @"%@%@", @"OtherSnapmaker_OrcaInstanceMessage", nsver];
	[[NSDistributedNotificationCenter defaultCenter] postNotificationName:notifname object:nil userInfo:[NSDictionary dictionaryWithObject:nsmsg forKey:@"data"] deliverImmediately:YES];
}

void send_message_mac_closing(const std::string &msg, const std::string &version)
{
	NSString *nsmsg = [NSString stringWithUTF8String:msg.c_str()];
	if (nsmsg == nil)
		return;
	//NSString *nsver = @"OtherSnapmaker_OrcaInstanceMessage" + [NSString stringWithCString:version.c_str() encoding:[NSString defaultCStringEncoding]];
	NSString *nsver = [NSString stringWithUTF8String:version.c_str()];
	NSString *notifname = [NSString stringWithFormat: @"%@%@", @"OtherSnapmaker_OrcaInstanceClosing", nsver];
	[[NSDistributedNotificationCenter defaultCenter] postNotificationName:notifname object:nil userInfo:[NSDictionary dictionaryWithObject:nsmsg forKey:@"data"] deliverImmediately:YES];
}

namespace GUI {
void OtherInstanceMessageHandler::register_for_messages(const std::string &version_hash)
{
	m_impl_osx = [[OtherInstanceMessageHandlerMac alloc] init];
	if(m_impl_osx) {
		NSString *nsver = [NSString stringWithUTF8String:version_hash.c_str()];
		[(id)m_impl_osx add_observer:nsver];
	}
}

void OtherInstanceMessageHandler::unregister_for_messages()
{
	//NSLog(@"unreegistering other instance messages");
	if (m_impl_osx) {
        // Stop deliveries before the observer goes away; a message from another instance arriving
        // during shutdown would otherwise reach a released object.
        [[NSDistributedNotificationCenter defaultCenter] removeObserver:(id)m_impl_osx];
        [(id)m_impl_osx release];
        m_impl_osx = nullptr;
    } else {
		NSLog(@"warning: unregister instance InstanceCheck notifications not required");
	}
}

void OtherInstanceMessageHandler::bring_instance_forward()
{
	if (m_impl_osx) {
		[(id)m_impl_osx bring_forward];
	}
}
}//namespace GUI
}//namespace Slicer



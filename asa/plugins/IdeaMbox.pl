use strict;
use LWP::UserAgent;
use HTTP::Request::Common qw(GET POST);
use HTTP::Response;
use Crypt::SSLeay; # warn NOW, not when logging in
use Unicode::Lite;

my $PLUGIN_NAME			= "IdeaMbox";
my $PLUGIN_VERSION		= "0.3";
my $DEFAULT_PRIO		= 3;
my $DEST_NUMBERS_REGEXP		=  '^\+[0-9]+'; # needed linke this in //, BUT it's not for everyone (only ideowcy maj± dowsz±d)

use vars qw ( %IdeaMbox_accounts );

# RULES:
# 1. ALWAYS, ALWAYS, ALWAYS create new presence/message/iq
# 2. Create handlers, plugin is named ser.vi.ce/Plugin
# 3. Provide ALL info into %plugin_data

RegisterEvent('iq','^'.$config{service_name}."/".$PLUGIN_NAME.'$',\&IdeaMbox_InIQ); # for registration
RegisterEvent('pre_connect','.+',\&IdeaMbox_PreConn); # for loading some data ;)

$plugin_data{$PLUGIN_NAME}->{inmessage_h}=\&IdeaMbox_InMessage;
$plugin_data{$PLUGIN_NAME}->{will_take_h}=\&IdeaMbox_WillTake;
$plugin_data{$PLUGIN_NAME}->{default_prio}=$DEFAULT_PRIO;
$plugin_data{$PLUGIN_NAME}->{version}=$PLUGIN_VERSION;
$plugin_data{$PLUGIN_NAME}->{needs_reg}=1;

sub IdeaMbox_WillTake {
        my $from=shift;
        my $to_nr=shift; # if not provided, it means it asks for the service, w/o number served by it 
        return (($to_nr?($to_nr =~ /$DEST_NUMBERS_REGEXP/):1)&&(exists($IdeaMbox_accounts{$from})));
};

sub IdeaMbox_PreConn {
#	%IdeaMbox_accounts=%{LoadXMLHash('accounts',$config{plugins}->{$PLUGIN_NAME}->{accounts_file},$PLUGIN_NAME)};
	my $t=tie(%IdeaMbox_accounts,"MLDBM::Sync",$config{plugins}->{$PLUGIN_NAME}->{accounts_file},O_CREAT|O_RDWR,0600);
#	$t->SyncCacheSize($config{numbers_tiehash_cache}) TODO eventually
	log2($PLUGIN_NAME.": Ready.");
};

sub IdeaMbox_InIQ {
	my $iq_o=shift;
	my $xmlns=shift;
	my $query_o=$iq_o->GetQuery();
	my $type=$iq_o->GetType();
	my $from=$iq_o->GetFrom(); my $to=$iq_o->GetTo();
	my $iq=new Net::Jabber::IQ;
	$iq->SetFrom($to); $iq->SetTo($from); $iq->SetID($iq_o->GetID);
	my $query=$iq->NewQuery($xmlns);

	if ($xmlns eq 'jabber:iq:register') {
		my $base_from=$from; $base_from=~s/\/.+$//g if ($base_from =~ /\//);
		if ($type eq 'get') {
			$iq->SetType('result');
			$query->SetInstructions('Send a message to: +48TELNUMBER@'.$config{service_name}." to send an SMS.\n".
				"You can add a contact like this to your roster.\n\n".
				"Provide your Username and Password to ".$PLUGIN_NAME." gateway.\n".
				"See http://www.idea.pl/ for info.");
			$query->SetUsername( ($IdeaMbox_accounts{$base_from}?$IdeaMbox_accounts{$base_from}->{username}:'') );
			$query->SetPassword('');
			$Connection->Send($iq);
		} elsif ($type eq 'set') {
			if (($query_o->GetUsername() eq '')&&($query_o->GetPassword() eq '')) {
				SendPluginPresencesToUser($PLUGIN_NAME,'unavailable',$from);
				delete $IdeaMbox_accounts{$base_from};
			} else {
				my $a=$IdeaMbox_accounts{$base_from};
				$a->{username}=$query_o->GetUsername();
				$a->{password}=$query_o->GetPassword();
				$IdeaMbox_accounts{$base_from}=$a;
				SendPluginPresencesToUser($PLUGIN_NAME,'available',$from);
				PushAgentToUsersRoster($from); # important in any plugin which needs registration!
			};
#			SaveXMLHash(\%IdeaMbox_accounts,'accounts',$config{plugins}->{$PLUGIN_NAME}->{accounts_file},$PLUGIN_NAME);
			$iq->SetType('result');
			$Connection->Send($iq);
		};
	};
};

sub IdeaMbox_InMessage {
	my $message_o=shift;
	my $type=shift;
	my $to=$message_o->GetTo(); my $from=$message_o->GetFrom();
	my $message=new Net::Jabber::Message;
	$message->SetTo($from); $message->SetFrom($to); $message->SetType($type);
	my $SN=$config{service_name};
	my $PN=$SN."/".$PLUGIN_NAME;
	my $base_from=$from; $base_from=~s/\/.+$//g if ($base_from =~ /\//);
	my $nr=$to; $nr=~s/@.+$//g;
	return unless ($to =~ /$PN$/); # shouldn't happen

	# the guts:

	my $tekst=convert('utf8','latin2',$message_o->GetBody());
	$tekst=~tr/±æê³ñó¶¿¼¡ÆÊ£ÑÓ¦¯¬/acelnoszzACELNOSZZ/;

	my $result_message="";

	my $ua=new LWP::UserAgent;
	$ua->agent("Lynx/2.8.5rel.1 libwww-FM/2.14 SSL-MM/1.4.1 OpenSSL/0.9.7d");
	$ua->timeout(60); # quickly please!
	$ua->no_proxy('www.idea.pl');
	# login:
	my $post_req=POST("https://www.idea.pl/portal/map/map/?_DARGS=/portal/layoutTemplates/html/header_files/login.jsp",[
		_dyncharset=>"ISO8859_2",
		login=>$IdeaMbox_accounts{$base_from}->{username},
		"_D:login"=>" ",
		password=>$IdeaMbox_accounts{$base_from}->{password},
		"_D:password"=>" ",
		"/amg/ptk/map/core/formhandlers/AdvancedProfileFormHandler.login.x"=>"0",
		"/amg/ptk/map/core/formhandlers/AdvancedProfileFormHandler.login.y"=>"0",
		"_D:/amg/ptk/map/core/formhandlers/AdvancedProfileFormHandler.login"=>" ",
		"/amg/ptk/map/core/formhandlers/AdvancedProfileFormHandler.extractDefaultValuesFromProfile"=>"false",
		"_D:/amg/ptk/map/core/formhandlers/AdvancedProfileFormHandler.extractDefaultValuesFromProfile"=>" ",
		"/amg/ptk/map/core/formhandlers/AdvancedProfileFormHandler.loginSuccessURL"=>"http://www.idea.pl/portal/map/map/pim",
		"_D:/amg/ptk/map/core/formhandlers/AdvancedProfileFormHandler.loginSuccessURL"=>" ",
		"_DARGS"=>"/portal/layoutTemplates/html/header_files/login.jsp",
		""=>"nullOption"
		]);
	$post_req->header('Accept','*/*');
	$post_req->header('Accept-Charset','us-ascii, ISO-8859-1, ISO-8859-2, ISO-8859-3, ISO-8859-4, ISO-8859-5, '.
		'ISO-8859-6, ISO-8859-7, ISO-8859-8, ISO-8859-9, ISO-8859-10, ISO-8859-13, ISO-8859-14, ISO-8859-15, '.
		'ISO-8859-16, windows-1250, windows-1251, windows-1252, windows-1256, windows-1257, cp437, cp737, cp850, '.
		'cp852, cp866, x-cp866-u, x-mac, x-mac-ce, x-kam-cs, koi8-r, koi8-u, koi8-ru, TCVN-5712, VISCII, utf-8');
	$post_req->header('Host','www.idea.pl');
	my $r=$ua->request($post_req); # do login
#	log4($PLUGIN_NAME.": login: [".$r->status_line."]");
	if ($r->status_line !~ /302 Moved Temp/) {
		$result_message="http request failed";
	} else {
		# cookies:
		my $msc=(grep(/^Set-Cookie: mapSecurityCookie=/,split(/\n/,$r->headers->as_string)))[0];
		my $mpc=(grep(/^Set-Cookie: mapProfileCookie=/,split(/\n/,$r->headers->as_string)))[0];
		if ((!$msc)||(!$mpc)||($msc eq '')||($mpc eq '')) {
			$result_message="login failed (bad username/password?)";
		} else {
			my $sid=(grep(/^Set-Cookie: SID=/,split(/\n/,$r->headers->as_string)))[0];
			my $vid=(grep(/^Set-Cookie: VisitorId=/,split(/\n/,$r->headers->as_string)))[0];
			$msc=~s/^Set-Cookie: mapSecurityCookie=//g; $msc=~s/;.+$//g;
			$mpc=~s/^Set-Cookie: mapProfileCookie=//g; $mpc=~s/;.*$//g;
			$sid=~s/^Set-Cookie: SID=//g; $sid=~s/;.+$//g;
			$vid=~s/^Set-Cookie: VisitorId=//g; $vid=~s/;.*$//g;
			# and post sms:
			$post_req=POST("http://www.idea.pl/portal/map/map/message_box?_DARGS=/gear/mapmessagebox/smsform.jsp",[
				"_dyncharset"=>"ISO8859_2",
				"/amg/ptk/map/messagebox/formhandlers/MessageFormHandler.type"=>"sms",
				"_D:/amg/ptk/map/messagebox/formhandlers/MessageFormHandler.type"=>" ",
				"enabled"=>"true",
				"/amg/ptk/map/messagebox/formhandlers/MessageFormHandler.errorURL"=>"/portal/map/map/message_box?mbox_view=newsms",
				"_D:/amg/ptk/map/messagebox/formhandlers/MessageFormHandler.errorURL"=>" ",
				"/amg/ptk/map/messagebox/formhandlers/MessageFormHandler.successURL"=>"/portal/map/map/message_box?mbox_view=messageslist",
				"_D:/amg/ptk/map/messagebox/formhandlers/MessageFormHandler.successURL"=>" ",
				"/amg/ptk/map/messagebox/formhandlers/MessageFormHandler.to"=>$nr,
				"_D:/amg/ptk/map/messagebox/formhandlers/MessageFormHandler.to"=>" ",
				"_D:/amg/ptk/map/messagebox/formhandlers/MessageFormHandler.body"=>" ",
				"/amg/ptk/map/messagebox/formhandlers/MessageFormHandler.body"=>$tekst,
				"counter"=>"640",
				"smscounter"=>"1",
				"/amg/ptk/map/messagebox/formhandlers/MessageFormHandler.create.x"=>"0",
				"/amg/ptk/map/messagebox/formhandlers/MessageFormHandler.create.y"=>"0",
				"_D:/amg/ptk/map/messagebox/formhandlers/MessageFormHandler.create"=>" ",
				"_DARGS"=>"/gear/mapmessagebox/smsform.jsp"
				]);
			$post_req->header('Accept','*/*');
			$post_req->header('Accept-Charset','us-ascii, ISO-8859-1, ISO-8859-2, ISO-8859-3, ISO-8859-4, ISO-8859-5, '.
				'ISO-8859-6, ISO-8859-7, ISO-8859-8, ISO-8859-9, ISO-8859-10, ISO-8859-13, ISO-8859-14, ISO-8859-15, '.
				'ISO-8859-16, windows-1250, windows-1251, windows-1252, windows-1256, windows-1257, cp437, cp737, cp850, '.
				'cp852, cp866, x-cp866-u, x-mac, x-mac-ce, x-kam-cs, koi8-r, koi8-u, koi8-ru, TCVN-5712, VISCII, utf-8');
			$post_req->header('Host','www.idea.pl');
			$post_req->header('Cookie',"mapProfileCookie=$mpc; mapSecurityCookie=$msc; SID=$sid; VisitorId=$vid;");
			$r=$ua->request($post_req);
			if ($r->status_line =~ /302 Moved Temp/) {
				my $get_req=GET("http://www.idea.pl".$r->header('Location'));
				$get_req->header('Accept','*/*');
				$get_req->header('Accept-Charset','us-ascii, ISO-8859-1, ISO-8859-2, ISO-8859-3, ISO-8859-4, ISO-8859-5, '.
				'ISO-8859-6, ISO-8859-7, ISO-8859-8, ISO-8859-9, ISO-8859-10, ISO-8859-13, ISO-8859-14, ISO-8859-15, '.
				'ISO-8859-16, windows-1250, windows-1251, windows-1252, windows-1256, windows-1257, cp437, cp737, cp850, '.
				'cp852, cp866, x-cp866-u, x-mac, x-mac-ce, x-kam-cs, koi8-r, koi8-u, koi8-ru, TCVN-5712, VISCII, utf-8');
				$get_req->header('Host','www.idea.pl');
				$get_req->header('Cookie',"mapProfileCookie=$mpc; mapSecurityCookie=$msc; SID=$sid; VisitorId=$vid;");
				$r=$ua->request($get_req); my $retrycounter=0;
				while (($r->status_line !~ /302 Moved Temp/)&&($retrycounter<3)) {
					$r=$ua->request($get_req); $retrycounter++;
				};
				{
					my ($tag,$now_err)=(0,0);
					my @a=split(/\n/,$r->content);
					foreach my $l (@a) {
						next unless ($l =~ m/Podsumowanie/ || $tag || $now_err);
						$tag++;
						$now_err=1 if ($tag>40);
						next if ($now_err && ($l !~ m/class=\"error\"/));
						$l=~s/<[^>]+>//g;
						next if ($l =~ m/^$/);
						$l=~s/\&nbsp;/\ /g;
						$result_message.=$l." ";
					};
#					print "IdeaDEBUG: ----------\n";
#					foreach my $l (@a) { print "IdeaDEBUG: ".$l."\n"; };
				};
				$result_message="Prawdopodobnie przeci±¿enie serwera (timeout przy oczekiwaniu na podsumowanie), spróbuj ponownie." if ($result_message !~ m/[a-z]/);
			} else {
				$result_message="SMS probably not sent";
			};
		};
	};

	$message->SetBody("www.idea.pl: ".convert("latin2","utf8",$result_message));
	$Connection->Send($message);
};

1;


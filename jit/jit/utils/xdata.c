/* --------------------------------------------------------------------------
   ICQ pthread jabber transport.

   x:data utils

   -------------------------------------------------------------------------- */

#include "jit/icqtransport.h"

xmlnode xdata_create( xmlnode q, char *type );
xmlnode xdata_insert_field( xmlnode q, char *type, char *var, char *label, char *data );
xmlnode xdata_insert_node( xmlnode q, char *name );
xmlnode xdata_insert_option( xmlnode q, char *label, char *data );
int xdata_test( xmlnode q, char *type );
char *xdata_get_data( xmlnode q, char *name );
xmlnode xdata_convert( xmlnode q, char *ns );

xmlnode xdata_create( xmlnode q, char *type )
{
    q = xmlnode_insert_tag( q, "x" );
    xmlnode_put_attrib( q, "type", type );
    xmlnode_put_attrib( q, "xmlns", "jabber:x:data" );
    return q;
}

xmlnode xdata_insert_field( xmlnode q, char *type, char *var, char *label, char *data )
{
    q = xmlnode_insert_tag( q, "field" );
    if( type != NULL )
	xmlnode_put_attrib( q, "type", type );
    if( var != NULL )
	xmlnode_put_attrib( q, "var", var );
    if( label != NULL )
	xmlnode_put_attrib( q, "label", label );
    xmlnode_insert_cdata( xmlnode_insert_tag( q, "value" ), data, -1 );
    return q;
}

xmlnode xdata_insert_option( xmlnode q, char *label, char *data )
{
    q = xmlnode_insert_tag( q, "option" );
    if( label != NULL )
	xmlnode_put_attrib( q, "label", label );
    xmlnode_insert_cdata( xmlnode_insert_tag( q, "value" ), data, -1 );
    return q;
}

int xdata_test( xmlnode q, char *type )
{
    if( (q = xmlnode_get_tag( q, "x" )) == NULL )
	return 0;
    if( j_strcmp( xmlnode_get_attrib( q, "xmlns" ), "jabber:x:data" ) != 0 )
	return 0;
    if( type == NULL )
	return 1;
    else
	return (j_strcmp( xmlnode_get_attrib( q, "type" ), type ) == 0);
}

xmlnode xdata_insert_node( xmlnode q, char *name )
{
    if( (q = xmlnode_get_tag( q, "x" )) == NULL )
	return NULL;
    if( j_strcmp( xmlnode_get_attrib( q, "xmlns" ), "jabber:x:data" ) != 0 )
	return NULL;
    return (xmlnode_insert_tag( q, name ));
}

char *xdata_get_data( xmlnode q, char *name )
{
    xmlnode qq;

    if( name == NULL )
	return NULL;
    if( (q = xmlnode_get_tag( q, "x" )) == NULL )
	return NULL;
    if( j_strcmp( xmlnode_get_attrib( q, "xmlns" ), "jabber:x:data" ) != 0 )
	return NULL;

    for( qq = xmlnode_get_firstchild( q ); qq != NULL; qq = xmlnode_get_nextsibling( qq ) )
    {
	if( j_strcmp( xmlnode_get_name( qq ), "field" ) == 0 )
	{
	    if( j_strcmp( xmlnode_get_attrib( qq, "var" ), name ) == 0 )
		return xmlnode_get_tag_data( qq, "value" );
	}
    }
    return NULL;
}

xmlnode xdata_convert( xmlnode q, char *ns )
{
    xmlnode node, qq;
    char *var;
    
    if( (q = xmlnode_get_tag( q, "x" )) == NULL )
	return q;
    if( j_strcmp( xmlnode_get_attrib( q, "xmlns" ), "jabber:x:data" ) != 0 )
	return q;

    node = xmlnode_new_tag( "query" );
    xmlnode_put_attrib( node, "xmlns" , ns );
    
    for( qq = xmlnode_get_firstchild( q ); qq != NULL; qq = xmlnode_get_nextsibling( qq ) )
    {
	if( j_strcmp( xmlnode_get_name( qq ), "field" ) == 0 )
	{
	    if( (var = xmlnode_get_attrib( qq, "var" )) != NULL )
		xmlnode_insert_cdata( xmlnode_insert_tag( node, var ), xmlnode_get_tag_data( qq, "value" ), -1 );
	}
    }
    return node;
}

/* vi: set ts=2 shiftwidth=2 expandtab:
 *
 * Copyright (C) 2006-2007  Simon Baldwin (simon_baldwin@yahoo.com)
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307
 * USA
 */

#include <assert.h>
#include <stdlib.h>
#include <stdio.h>
#include <ctype.h>
#include <string.h>

#include <libxml/parser.h>
#include <libxml/tree.h>
#include <libxml/debugXML.h>

#include "protos.h"


/*
 * IFMES namespace and element/attribute name constants.  The following set
 * is generated from version 1.1, working draft "2 September 2005".
 */
static const struct {
  const xmlChar *const NAMESPACE, *const RDF, *const ABOUT, *const SEQ,
                *const BAG, *const DESCRIPTION, *const LI, *const RESOURCE;
} rdf_names = {
  .NAMESPACE = "http://www.w3.org/1999/02/22-rdf-syntax-ns#",
  .RDF = "RDF", .ABOUT = "about", .SEQ = "Seq", .BAG = "Bag",
  .DESCRIPTION = "Description", .LI = "li", .RESOURCE = "resource"
};

static const struct {
  const xmlChar *const NAMESPACE, *const AUTHOR, *const BYLINE, *const DESC,
                *const GENRE, *const HEADLINE, *const LENGTH, *const PUBLISHER,
                *const RELEASE_DATE, *const TITLE, *const VERSION;
} if_names = {
  .NAMESPACE = "http://purl.org/int-fiction/metadata/1.1/",
  .AUTHOR = "author", .BYLINE = "byline", .DESC = "desc", .GENRE = "genre",
  .HEADLINE = "headline", .LENGTH = "length", .PUBLISHER = "publisher",
  .RELEASE_DATE = "releaseDate", .TITLE = "title", .VERSION = "version"
};

static const struct {
  const xmlChar *const NAMESPACE, *const GAME;
} baf_names = {
  .NAMESPACE = "http://wurb.com/if/",
  .GAME = "game"
};

static const struct {
  const xmlChar *const NAMESPACE, *const STORY;
} ifm_names = {
  .NAMESPACE = "http://www.logicalshift.org.uk/IF/metadata/",
  .STORY = "story"
};

static const struct {
  const xmlChar *const NAMESPACE, *const DESCRIPTION, *const TITLE;
} dc_names = {
  .NAMESPACE = "http://purl.org/dc/elements/1.1/",
  .TITLE = "title", .DESCRIPTION = "description"
};


/*
 * iFiction namespace and element/attribute name constants.  The following set
 * is generated from "The Treaty of Babel, A community standard for IF biblio-
 * graphy", Draft 7, 13 April 2006.
 */
static const struct {
  const xmlChar *const NAMESPACE, *const IFINDEX;
} babel_names = {
  .NAMESPACE = "http://babel.ifarchive.org/protocol/iFiction/",
  .IFINDEX = "ifindex"
};

static const struct {
  const xmlChar *const NAMESPACE, *const STORY, *const IDENTIFICATION,
                *const BIBLIOGRAPHIC, *const RESOURCES, *const CONTACTS,
                *const COVER, *const RELEASES, *const COLOPHON,
                *const ANNOTATION, *const ZCODE, *const TADS2, *const TADS3,
                *const GLULX, *const HUGO, *const ADRIFT;
} story_names = {
  .NAMESPACE = NULL, .STORY = "story", .IDENTIFICATION = "identification",
  .BIBLIOGRAPHIC = "bibliographic", .RESOURCES = "resources",
  .CONTACTS = "contacts", .COVER = "cover", .RELEASES = "releases",
  .COLOPHON = "colophon", .ANNOTATION = "annotation", .ZCODE = "zcode",
  .TADS2 = "tads2", .TADS3 = "tads3", .GLULX = "glulx", .HUGO = "hugo",
  .ADRIFT = "adrift",
};

static const struct {
  const xmlChar *const NAMESPACE, *const IFID, *const FORMAT, *const BAFN;
} ident_names = {
  .NAMESPACE = NULL, .IFID = "ifid", .FORMAT = "format", .BAFN = "bafn"
};

static const struct {
  const xmlChar *const NAMESPACE, *const TITLE, *const AUTHOR, *const LANGUAGE,
                *const HEADLINE, *const FIRSTPUBLISHED, *const GENRE,
                *const GROUP, *const FORGIVENESS, *const DESCRIPTION,
                *const BR;
} biblio_names = {
  .NAMESPACE = NULL, .TITLE = "title", .AUTHOR = "author",
  .LANGUAGE = "language", .HEADLINE = "headline",
  .FIRSTPUBLISHED = "firstpublished", .GENRE = "genre", .GROUP = "group",
  .FORGIVENESS = "forgiveness", .DESCRIPTION = "description", .BR = "br"
};

static const struct {
  const xmlChar *const NAMESPACE, *const RELEASEDATE, *const VERSION,
                *const RELEASE;
} vm_names = {
  .NAMESPACE = NULL, .RELEASEDATE = "releasedate", .VERSION = "version",
  .RELEASE = "release"
};

static const struct {
  const xmlChar *const NAMESPACE, *const GAMEBOX;
} annotation_names = {
  .NAMESPACE = NULL, .GAMEBOX = "gamebox"
};

static const struct {
  const xmlChar *const NAMESPACE, *const ABOUT, *const GROUP;
} gamebox_names = {
  .NAMESPACE = NULL, .ABOUT = "about", .GROUP = "group"
};


/* Binding structures to allow variables to be bound to the above names. */
typedef struct {
  const xmlChar *name;
  const xmlChar *namespace;
  char ** const variable_ptr;
} binding_t;

typedef struct {
  const xmlChar *parent_name;
  const xmlChar *parent_namespace;
  const xmlChar *name;
  const xmlChar *namespace;
  char ** const variable_ptr;
} double_binding_t;


/*
 * xml_node_element_matches()
 *
 * Return true if the given node is an element that matches the name and
 * namespace, if any, passed in.  Helper for finding nodes of interest.
 */
static int
xml_node_element_matches (const xmlNodePtr node,
                          const xmlChar *name, const xmlChar *namespace)
{
  return node->type == XML_ELEMENT_NODE
      && xmlStrcmp (node->name, name) == 0
      && (!namespace
          || (node->ns && xmlStrcmp (node->ns->href, namespace) == 0));
}


/*
 * xml_node_find_child_element()
 *
 * Return the first child of the given node that matches the name and
 * namespace, if any, passed in.  Helper for finding nodes of interest.
 */
static xmlNodePtr
xml_node_find_child_element (const xmlNodePtr node,
                             const xmlChar *name, const xmlChar *namespace)
{
  xmlNodePtr child;

  for (child = node->xmlChildrenNode; child; child = child->next)
    {
      if (xml_node_element_matches (child, name, namespace))
        break;
    }

  return child;
}


/*
 * xml_get_rdf_node_property()
 *
 * Find a property value for a given RDF node.
 *
 * RDF can store its data in either attributes or child elements.  To handle
 * this, first scan the node's attributes for matching values, then scan for
 * for child elements whose names match the requirements.  All matches are
 * concatenated together into a comma-separated string.
 *
 * If the property is not found, return NULL.
 */
static xmlChar *
xml_get_rdf_node_property (const xmlNodePtr node,
                           const xmlChar *name, const xmlChar *namespace)
{
  xmlChar *property;
  xmlNodePtr child;

  property = xmlGetNsProp (node, name, namespace);

  for (child = node->xmlChildrenNode; child; child = child->next)
    {
      if (xml_node_element_matches (child, name, namespace))
        {
          xmlChar *content;

          content = xmlNodeListGetString (child->doc,
                                          child->xmlChildrenNode, TRUE);
          if (content)
            {
              if (property)
                {
                  property = xmlStrcat (property, (xmlChar *) ", ");
                  property = xmlStrcat (property, content);
                }
              else
                property = xmlStrdup (content);

              xmlFree (content);
            }
        }
    }

  return property;
}


/*
 * xml_get_rdf_node_properties()
 *
 * Process multiple RDF properties using a given binding.  The function
 * converts from UTF-8 to ISO-8859-1 for each bound property.
 *
 * If node is NULL, default to setting all bindings to NULL.  It is not
 * an error for node to be NULL.
 */
static void
xml_get_rdf_node_properties (const xmlNodePtr node, const binding_t *bindings)
{
  const binding_t *binding;

  for (binding = bindings; binding->variable_ptr; binding++)
    *binding->variable_ptr = NULL;

  if (node)
    {
      for (binding = bindings; binding->variable_ptr; binding++)
        {
          xmlChar *property;

          property = xml_get_rdf_node_property (node,
                                                binding->name,
                                                binding->namespace);
          if (property)
            {
              memory_free (*binding->variable_ptr);
              *binding->variable_ptr = utf_utf8_to_iso8859 (property);
              xmlFree (property);
            }
        }
    }
}


/*
 * xml_parse_rdf_game_element()
 *
 * Parse a BAF:game or IFM:story node.  Extract the RDF properties and add
 * an entry to the game set to represent the node.
 */
static void
xml_parse_rdf_game_element (const xmlNodePtr node)
{
  char *about;         /* RDF:about */
  char *author;        /* IF:author (used if no byline) */
  char *byline;        /* IF:byline (overrides author) */
  char *description;   /* IF:desc (overrides headline) */
  char *genre;         /* IF:genre */
  char *headline;      /* IF:headline (used if no desc) */
  char *length;        /* IF:length */
  char *publisher;     /* IF:publisher (used if no author) */
  char *release_date;  /* IF:releaseDate */
  char *title;         /* IF:title */
  char *version;       /* IF:version */

  binding_t bindings[] = {
    { rdf_names.ABOUT, rdf_names.NAMESPACE, &about },
    { if_names.AUTHOR, if_names.NAMESPACE, &author },
    { if_names.BYLINE, if_names.NAMESPACE, &byline },
    { if_names.DESC, if_names.NAMESPACE, &description },
    { if_names.GENRE, if_names.NAMESPACE, &genre },
    { if_names.HEADLINE, if_names.NAMESPACE, &headline },
    { if_names.LENGTH, if_names.NAMESPACE, &length },
    { if_names.PUBLISHER, if_names.NAMESPACE, &publisher },
    { if_names.RELEASE_DATE, if_names.NAMESPACE, &release_date },
    { if_names.TITLE, if_names.NAMESPACE, &title },
    { if_names.VERSION, if_names.NAMESPACE, &version },
    { NULL, NULL, NULL }
  };

  xml_get_rdf_node_properties (node, bindings);

  if (about)
    {
      if (!title)
        title = memory_strdup ("[Unknown Game]");

      gameset_add_game (about, author, byline, description, genre, headline,
                        length, publisher, release_date, title, version);
    }

  memory_free (about);
  memory_free (author);
  memory_free (byline);
  memory_free (description);
  memory_free (genre);
  memory_free (headline);
  memory_free (length);
  memory_free (publisher);
  memory_free (release_date);
  memory_free (title);
  memory_free (version);
}


/*
 * xml_get_rdf_container()
 *
 * For a given RDF:Seq or RDF:Bag container node, return the peer that offers
 * the title and description properties.
 */
static xmlNodePtr
xml_get_rdf_container (const xmlNodePtr node)
{
  xmlChar *about;
  xmlNodePtr descriptor;

  descriptor = NULL;

  /*
   * Find out what this container node describes, then search peers for the
   * RDF:Description nodes that match our's.
   */
  about = xml_get_rdf_node_property (node,
                                     rdf_names.ABOUT,
                                     rdf_names.NAMESPACE);
  if (about)
    {
      for (descriptor = node->parent->xmlChildrenNode;
           descriptor; descriptor = descriptor->next)
        {
          if (xml_node_element_matches (descriptor,
                                        rdf_names.DESCRIPTION,
                                        rdf_names.NAMESPACE))
            {
              xmlChar *property;

              property = xml_get_rdf_node_property (descriptor,
                                                    rdf_names.ABOUT,
                                                    rdf_names.NAMESPACE);
              if (property)
                {
                  if (xmlStrcmp (about, property) == 0)
                    {
                      xmlFree (property);
                      break;
                    }

                  xmlFree (property);
                }
            }
        }

      xmlFree (about);
    }

  return descriptor;
}


/*
 * xml_add_rdf_container_resources()
 *
 * Add RDF:Seq or RDF:Bag resources to a game group.  Searches the given node
 * for RDF:li elements, and uses their RDF:resource property as the resource
 * to add to the game group.
 */
static void
xml_add_rdf_container_resources (const xmlNodePtr node, groupref_t group)
{
  char *resource;  /* RDF:resource */
  xmlNodePtr child;

  binding_t bindings[] = {
    { rdf_names.RESOURCE, rdf_names.NAMESPACE, &resource },
    { NULL, NULL, NULL }
  };

  for (child = node->xmlChildrenNode; child; child = child->next)
    {
      if (xml_node_element_matches (child, rdf_names.LI, rdf_names.NAMESPACE))
        {
          xml_get_rdf_node_properties (child, bindings);

          if (resource)
            {
              int status;

              status = gamegroup_add_game_to_group (group, resource);
              if (!status)
                status = gamegroup_add_group_to_group (group, resource);

              memory_free (resource);
            }
        }
    }
}


/*
 * xml_parse_rdf_container_element()
 *
 * Parse an RDF:Seq or RDF:Bag node, defining a group of games.  Extract the
 * RDF properties and add an entry to the game group set to represent the node.
 * The resources in the container aren't yet added; this has to happen later,
 * so that forward references to other containers work correctly.
 */
static groupref_t
xml_parse_rdf_container_element (const xmlNodePtr node)
{
  char *about;        /* RDF:about */
  char *title;        /* DC:title */
  char *description;  /* DC:description */
  xmlNodePtr descriptor;
  groupref_t group;

  binding_t bindings[] = {
    { rdf_names.ABOUT, rdf_names.NAMESPACE, &about },
    { dc_names.TITLE, dc_names.NAMESPACE, &title },
    { dc_names.DESCRIPTION, dc_names.NAMESPACE, &description },
    { NULL, NULL, NULL }
  };

  descriptor = xml_get_rdf_container (node);
  xml_get_rdf_node_properties (descriptor, bindings);

  if (about)
    {
      if (!title)
        title = memory_strdup ("[Unknown Game Group]");

      group = gamegroup_add_group (about, title, description);
    }
  else
    group = NULL;

  memory_free (about);
  memory_free (title);
  memory_free (description);

  return group;
}


/*
 * xml_parse_rdf_root_element()
 *
 * Parse the RDF root element, in IFMES, an RDF:RDF node, searching first for
 * BAF:game or IFM:story elements, and parsing each found, then searching
 * for RDF:Seq or RDF:Bag elements that might group games into sequences.
 */
static void
xml_parse_rdf_root_element (const xmlNodePtr node)
{
  typedef struct {
    xmlNodePtr node;
    groupref_t group;
  } pair_t;
  xmlNodePtr child;
  vectorref_t pairs;
  int index_;

  for (child = node->xmlChildrenNode; child; child = child->next)
    {
      if (xml_node_element_matches (child,
                                    baf_names.GAME,
                                    baf_names.NAMESPACE)
          || xml_node_element_matches (child,
                                       ifm_names.STORY,
                                       ifm_names.NAMESPACE))
        xml_parse_rdf_game_element (child);
    }

  /*
   * Create an array of sequence child nodes and their associated groups; this
   * is used later to fill the groups with their contents.
   */
  pairs = vector_create (sizeof (pair_t));

  for (child = node->xmlChildrenNode; child; child = child->next)
    {
      if (xml_node_element_matches (child,
                                    rdf_names.SEQ,
                                    rdf_names.NAMESPACE)
          || xml_node_element_matches (child,
                                       rdf_names.BAG,
                                       rdf_names.NAMESPACE))
        {
          groupref_t group;

          group = xml_parse_rdf_container_element (child);
          if (group)
            {
              pair_t pair;

              pair.node = child;
              pair.group = group;
              vector_append (pairs, &pair);
            }
        }
    }

  /*
   * Now we have found and assigned all of the groups, run through group-node
   * pairings and add the container resources for each.
   */
  for (index_ = 0; index_ < vector_get_length (pairs); index_++)
    {
      pair_t pair;

      vector_get (pairs, index_, &pair);
      xml_add_rdf_container_resources (pair.node, pair.group);
    }

  vector_destroy (pairs);
}


/*
 * xml_get_babel_node_property()
 *
 * Find a property value for a given iFiction node.
 *
 * iFiction uses pure child elements, so this function simply searches the
 * children for the first name and namespace match found.
 *
 * If the property is not found, return NULL.
 */
static xmlChar *
xml_get_babel_node_property (const xmlNodePtr node,
                             const xmlChar *name, const xmlChar *namespace)
{
  xmlNodePtr child;

  child = xml_node_find_child_element (node, name, namespace);
  if (child)
    {
      xmlChar *property;

      property = xmlNodeListGetString (child->doc,
                                       child->xmlChildrenNode, TRUE);
      return property;
    }

  return NULL;
}


/*
 * xml_get_babel_node_properties()
 *
 * Process multiple iFiction properties using a given double binding.  The
 * function converts from UTF-8 to ISO-8859-1 for each bound property.
 */
static void
xml_get_babel_node_properties (const xmlNodePtr node,
                               const double_binding_t *bindings)
{
  const double_binding_t *binding;

  for (binding = bindings; binding->variable_ptr; binding++)
    *binding->variable_ptr = NULL;

  for (binding = bindings; binding->variable_ptr; binding++)
    {
      xmlNodePtr child;

      child = xml_node_find_child_element (node,
                                           binding->parent_name,
                                           binding->parent_namespace);
      if (child)
        {
          xmlChar *property;

          property = xml_get_babel_node_property (child,
                                                  binding->name,
                                                  binding->namespace);
          if (property)
            {
              memory_free (*binding->variable_ptr);
              *binding->variable_ptr = utf_utf8_to_iso8859 (property);
              xmlFree (property);
            }
        }
    }
}


/*
 * xml_get_babel_story_description()
 *
 * Specialized property retrieval for an iFiction description node.
 *
 * To retain <br/> elements, a description node need to have its children
 * concatenated, and <br/> elements written as text for later normalization.
 *
 * If the property is not found, return NULL.
 */
static void
xml_get_babel_story_description (const xmlNodePtr node, char **description)
{
  xmlNodePtr first, second;

  *description = NULL;

  first = xml_node_find_child_element (node,
                                       story_names.BIBLIOGRAPHIC,
                                       story_names.NAMESPACE);
  if (first)
    {
      second = xml_node_find_child_element (first,
                                            biblio_names.DESCRIPTION,
                                            biblio_names.NAMESPACE);
      if (second)
        {
          xmlChar *property;
          xmlNodePtr child;

          property = NULL;

          for (child = second->xmlChildrenNode; child; child = child->next)
            {
              xmlChar *content;

              content = NULL;

              if (child->type == XML_TEXT_NODE)
                content = xmlNodeGetContent (child);

              else if (xml_node_element_matches (child,
                                                 biblio_names.BR,
                                                 biblio_names.NAMESPACE))
                content = xmlStrdup ("<br/>");

              if (content)
                {
                  if (property)
                    property = xmlStrcat (property, content);
                  else
                    property = xmlStrdup (content);

                  xmlFree (content);
                }
            }

          if (property)
            {
              memory_free (*description);
              *description = utf_utf8_to_iso8859 (property);
              xmlFree (property);
            }
        }
    }
}


/*
 * xml_normalize_babel_description()
 *
 * Normalize an iFiction description string, changing <br/> to newline,
 * compressing multiple whitespace to a single whitespace, and removing any
 * leading and trailing whitespace.
 */
static void
xml_normalize_babel_description (char *description)
{
  int index_, length;

  length = strlen (description);

  for (index_ = 0; index_ < length; index_++)
    {
      if (isspace (description[index_]))
        description[index_] = ' ';
    }

  for (index_ = 0; index_ < length - 1; index_++)
    {
      int extent;

      extent = strspn (description + index_, " ");
      if (extent > 1)
        {
          memmove (description + index_ + 1,
                   description + index_ + extent, length - index_ - extent + 1);
          length -= extent - 1;
        }
    }

  if (isspace (description[0]))
    {
      memmove (description, description + 1, length);
      length--;
    }
  if (isspace (description[length - 1]))
    {
      description[length - 1] = '\0';
      length--;
    }

  for (index_ = 0; index_ < length; index_++)
    {
      if (strncmp (description + index_, "<br/>", 5) == 0)
        {
          description[index_] = '\n';
          memmove (description + index_ + 1,
                   description + index_ + 5, length - index_ - 4);
          length -= 4;
        }
    }
}


/*
 * xml_parse_babel_story_element()
 *
 * Parse an iFiction story node.  Extract the story properties and add an
 * entry to the game set to represent the node.  Creates 'about' using the
 * <annotation><gamebox><about> element, or download and IFID.
 */
static void
xml_parse_babel_story_element (const xmlNodePtr node, const char *download)
{
  char *about;         /* <annotation><gamebox><about>, or url+IFID */
  char *ifid;          /* <identification><ifid> */
  char *author;        /* <bibliographic><author> */
  char *byline;        /* NULL */
  char *description;   /* <bibliographic><description> */
  char *genre;         /* <bibliographic><genre> */
  char *headline;      /* <bibliographic><headline> (used if no desc) */
  char *length;        /* NULL */
  char *publisher;     /* <bibliographic><group>(!) */
  char *release_date;  /* <bibliographic><firstpublished>,
                          or <tads?><releasedate> */
  char *title;         /* <bibliographic><title> */
  char *version;       /* NULL, or <tads?><version>|<zcode|glulx><release> */
  char *group;         /* <annotation><gamebox><group> */
  xmlNodePtr child;

  double_binding_t bindings[] = {
    { story_names.IDENTIFICATION, story_names.NAMESPACE,
        ident_names.IFID, ident_names.NAMESPACE, &ifid },
    { story_names.BIBLIOGRAPHIC, story_names.NAMESPACE,
        biblio_names.AUTHOR, biblio_names.NAMESPACE, &author },
    { story_names.BIBLIOGRAPHIC, story_names.NAMESPACE,
        biblio_names.GENRE, biblio_names.NAMESPACE, &genre },
    { story_names.BIBLIOGRAPHIC, story_names.NAMESPACE,
        biblio_names.HEADLINE, biblio_names.NAMESPACE, &headline },
    { story_names.BIBLIOGRAPHIC, story_names.NAMESPACE,
        biblio_names.FIRSTPUBLISHED, biblio_names.NAMESPACE, &release_date },
    { story_names.BIBLIOGRAPHIC, story_names.NAMESPACE,
        biblio_names.TITLE, biblio_names.NAMESPACE, &title },
    { story_names.BIBLIOGRAPHIC, story_names.NAMESPACE,
        biblio_names.GROUP, biblio_names.NAMESPACE, &publisher },

    { story_names.TADS2, story_names.NAMESPACE,
        vm_names.RELEASEDATE, vm_names.NAMESPACE, &release_date },
    { story_names.TADS2, story_names.NAMESPACE,
        vm_names.VERSION, vm_names.NAMESPACE, &version },
    { story_names.TADS3, story_names.NAMESPACE,
        vm_names.RELEASEDATE, vm_names.NAMESPACE, &release_date },
    { story_names.TADS3, story_names.NAMESPACE,
        vm_names.VERSION, vm_names.NAMESPACE, &version },
    { story_names.ZCODE, story_names.NAMESPACE,
        vm_names.RELEASE, vm_names.NAMESPACE, &version },
    { story_names.GLULX, story_names.NAMESPACE,
        vm_names.RELEASE, vm_names.NAMESPACE, &version },
    { NULL, NULL, NULL, NULL, NULL }
  };

  double_binding_t gamebox_bindings[] = {
    { annotation_names.GAMEBOX, annotation_names.NAMESPACE,
        gamebox_names.ABOUT, gamebox_names.NAMESPACE, &about },
    { annotation_names.GAMEBOX, annotation_names.NAMESPACE,
        gamebox_names.GROUP, gamebox_names.NAMESPACE, &group },
    { NULL, NULL, NULL, NULL, NULL }
  };

  byline = length = publisher = NULL;

  xml_get_babel_node_properties (node, bindings);
  xml_get_babel_story_description (node, &description);

  child = xml_node_find_child_element (node,
                                       story_names.ANNOTATION,
                                       story_names.NAMESPACE);
  if (child)
    xml_get_babel_node_properties (child, gamebox_bindings);
  else
    about = NULL;

  if (!about && ifid)
    {
      about = memory_malloc (strlen (download) + strlen (ifid) + 1);
      strcpy (about, download);
      strcat (about, ifid);
    }

  if (about)
    {
      if (!title)
        title = memory_strdup ("[Unknown Game]");

      if (description)
        xml_normalize_babel_description (description);

      if (author && strcasecmp (author, "Anonymous") == 0)
        {
          memory_free (author);
          author = NULL;
        }

      gameset_add_game (about, author, byline, description, genre, headline,
                        length, publisher, release_date, title, version);

      if (group)
        {
          groupref_t gamegroup;

          gamegroup = gamegroup_find_group (group);
          if (!gamegroup)
            gamegroup = gamegroup_add_group (group, group, NULL);

          gamegroup_add_game_to_group (gamegroup, about);
        }
    }

  memory_free (about);
  memory_free (ifid);
  memory_free (author);
  memory_free (byline);
  memory_free (description);
  memory_free (genre);
  memory_free (headline);
  memory_free (length);
  memory_free (publisher);
  memory_free (release_date);
  memory_free (title);
  memory_free (version);
}


/*
 * xml_parse_babel_root_element()
 *
 * Parse the ifindex root element, in iFiction, an ifindex node, searching
 * for story nodes within it.  The download base url is extracted from an
 * environment variable, defaulted to "http://babel.ifarchive.org/download/".
 */
static void
xml_parse_babel_root_element (const xmlNodePtr node)
{
  char *download;
  xmlNodePtr child;

  download = getenv ("GAMEBOX_BABEL_URL");
  if (!download)
    download = memory_strdup ("http://babel.ifarchive.org/download/");
  else
    download = memory_strdup (download);

  for (child = node->xmlChildrenNode; child; child = child->next)
    {
      if (xml_node_element_matches (child,
                                    story_names.STORY,
                                    story_names.NAMESPACE))
        xml_parse_babel_story_element (child, download);
    }

  memory_free (download);
}


/*
 * xml_parse_file()
 *
 * Parse the given XML file, if it contains a collection, into a set of
 * games.  A collection is defined by an RDF:RDF node in IFMES, and by a
 * ifindex node in iFiction.  Returns FALSE if not apparently a games
 * collection.
 */
int
xml_parse_file (const char *xml_file, int dump_document)
{
  xmlDocPtr document;
  xmlNodePtr node;
  void (*root_node_handler) (xmlNodePtr);

  document = xmlParseFile (xml_file);
  if (!document)
    return FALSE;

  if (dump_document)
    xmlDebugDumpDocument (stderr, document);

  node = xmlDocGetRootElement (document);
  if (xml_node_element_matches (node,
                                rdf_names.RDF,
                                rdf_names.NAMESPACE))
    root_node_handler = xml_parse_rdf_root_element;
  else if (xml_node_element_matches (node,
                                     babel_names.IFINDEX,
                                     babel_names.NAMESPACE))
    root_node_handler = xml_parse_babel_root_element;
  else
    root_node_handler = NULL;

  if (root_node_handler)
    {
      (*root_node_handler) (node);
      xmlFreeDoc (document);

      return TRUE;
    }

  xmlFreeDoc (document);
  return FALSE;
}
